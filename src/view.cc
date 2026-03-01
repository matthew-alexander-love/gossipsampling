/**
 * GossipSampling
 * Copyright (C) Matthew Love 2024 (gossipsampling@gmail.com)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <vector>
#include <unordered_map>
#include <deque>
#include <string>
#include <memory>
#include <random>
#include <mutex>
#include <queue>
#include <cstdint>

#include "node_descriptor.h"
#include "view.h"

namespace gossip {

std::string VectorLog::LogEntry::to_string() const {
    return "{ id: " + id + ", selected: " + selected + ", time: " + std::to_string(time) + " }";
}

void VectorLog::push_back(const std::string& id, const std::string& selected, uint64_t time) {
    std::lock_guard<std::mutex> lock(_lock);
    _log.push_back(LogEntry(id, selected, time));
}

std::vector<VectorLog::LogEntry> VectorLog::data_copy() {
    std::lock_guard<std::mutex> lock(_lock);
    return _log;
}

std::string VectorLog::to_string() const {
    std::lock_guard<std::mutex> lock(_lock);
    std::string output;
    for (auto entry : _log) {
        output += entry.to_string();
        output += ", ";
    }
    return output;
}

std::shared_ptr<NodeDescriptor> View::LoggedPeerSelector::select_peer() {
    std::shared_ptr<NodeDescriptor> selected = select_peer_impl();
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::string selected_address = "";
    if (selected) {
        selected_address = selected->address();
    }
    _log->push_back(_id, selected_address, ms);
    return selected;
}

std::shared_ptr<NodeDescriptor> URView::TailPeerSelector::select_peer_impl() {
    std::lock_guard<std::mutex> lock(_view->_lock);
    if (!_view->_view.empty()) {
        return _view->_view.back();
    }
    return nullptr;
}

std::string URView::TailPeerSelector::print() const {
    std::string str = "TailPeerSelector(Selecting: " + _view->_view.back()->print() +
    ", View: " + _view->print() + ")";
    return str;
}

std::shared_ptr<NodeDescriptor> URView::URPeerSelector::select_peer_impl() {
    std::lock_guard<std::mutex> lock(_view->_lock);
    std::uniform_int_distribution<> distr(0, _view->_view.size() - 1); // This has to be inside dummy, size could change leading to oom access
    //std::cout << "Selected Number UR select_peer_impl" << std::endl;
    if (!_view->_view.empty()) {
        return _view->_view[distr(_view->_eng)];
    }
    return nullptr;
}

std::string URView::URPeerSelector::print() const {
    std::string str = "URPeerSelector(Selecting: all, View: " + _view->print() + ")";
    return str;
}

URView::URNRPeerSelector::URNRPeerSelector(std::shared_ptr<URView> view) : _view(view) {
    std::lock_guard<std::mutex> lock(_view->_lock);
    for (auto node : _view->_view) {
        _qos_queue.push_back(node);
    }
}

std::shared_ptr<NodeDescriptor> URView::URNRPeerSelector::select_peer_impl() {
    { // Removing these scoped braces will cause deadlock
        std::lock_guard<std::mutex> lock(_view->_lock);
        while(!_qos_queue.empty()) {
            std::shared_ptr<NodeDescriptor> selected_peer = _qos_queue.front();
            if (_view->_node_lut.find(selected_peer->address()) != _view->_node_lut.end()) {
                _qos_queue.pop_front();
                return selected_peer;
            }
            _qos_queue.pop_front();
        }
    }
    return random_selection();
}

std::string URView::URNRPeerSelector::print() const {
    std::string str = "URNRPeerSelector(Selecting: ";
    for (auto node : _qos_queue) {
        str += node->print() + ", ";
    }
    str += ", View: " + _view->print() + ")";
    return str;
}

std::shared_ptr<NodeDescriptor> URView::URNRPeerSelector::random_selection() {
    std::uniform_int_distribution<> distr(0, _view->_view.size() - 1);
    std::lock_guard<std::mutex> lock(_view->_lock);
    if (!_view->_view.empty()) {
        return _view->_view[distr(_view->_eng)];
    }
    return nullptr;
}

void URView::URNRPeerSelector::permute_qos_queue() {
    std::shuffle(_qos_queue.begin(), _qos_queue.end(), _view->_eng);
}

void URView::URNRPeerSelector::notify_add(std::shared_ptr<NodeDescriptor> new_node) {
    _qos_queue.push_back(new_node);
    permute_qos_queue();
}

void URView::URNRPeerSelector::notify_add(std::vector<std::shared_ptr<NodeDescriptor>>& new_nodes) {
    for (auto& node : new_nodes) {
        //std::cout << "Notify add: Adding " << *node << " to urnr qos_queue";
        _qos_queue.push_back(node);
    }
    permute_qos_queue();
}

URView::URView(std::string address, int size, int healing, int swap) 
                    : _self(std::make_shared<NodeDescriptor>(address, 0)),
                    _size(size), _healing(healing), _swap(swap), _eng(_rd()), _selector(nullptr) {
    _node_lut[address] = _self;
}

void URView::init_selector(SelectorType type, std::shared_ptr<TSLog> log) {
    _selector = create_subscriber(type, log);
}

std::shared_ptr<NodeDescriptor> URView::select_peer() {
    {
        std::lock_guard<std::mutex> lock(_lock);
        if (!_selector) {
            return nullptr;
        }
    }
    return _selector->select_peer();
}

std::vector<std::shared_ptr<NodeDescriptor>> URView::tx_nodes() {
    std::vector<std::shared_ptr<NodeDescriptor>> buf;
    buf.push_back(_self);
    std::lock_guard<std::mutex> lock(_lock);
    permute();
    move_old_to_back(_healing);
    std::vector<std::shared_ptr<NodeDescriptor>> to_send = head((_size / 2) - 1);
    buf.insert(buf.end(), to_send.begin(), to_send.end());
    return buf;
}

void URView::rx_nodes(std::vector<std::shared_ptr<NodeDescriptor>>& nodes) {
    std::lock_guard<std::mutex> lock(_lock);
    append(nodes);
    // Dont need remove duplicates as duplicates are never added, just reset age
    remove_old(std::min(_healing, static_cast<int>(_view.size()) - _size));
    remove_head(std::min(_swap, static_cast<int>(_view.size()) - _size));
    remove_random(static_cast<int>(_view.size()) - _size);
}

std::vector<std::shared_ptr<NodeDescriptor>> URView::head(int num_get) const {
    if (num_get <= 0) {
        // ToDo: Add Logging
        return std::vector<std::shared_ptr<NodeDescriptor>>{};
    }
    if (num_get > _view.size()) {
        num_get = _view.size();
    }
    std::vector<std::shared_ptr<NodeDescriptor>> buf(_view.begin(), _view.begin() + num_get);
    return buf;
}

void URView::increment_age() {
    std::lock_guard<std::mutex> _(_lock);
    for (auto& node : _view) {
        node->age()++;
    }
}

void URView::append(std::shared_ptr<NodeDescriptor> new_peer) {
    if (_node_lut.find(new_peer->address()) == _node_lut.end()) {
        _view.push_back(new_peer);
        for (auto& sub : _subscribers) {
            sub->notify_add(new_peer);
        }
        _node_lut[new_peer->address()] = new_peer;
        
    }
    else {
        // Reset age of already known peer
        if (_node_lut[new_peer->address()]->age() > new_peer->age()) {
            _node_lut[new_peer->address()]->age() = new_peer->age();
        }
    }
}

void URView::append(std::vector<std::shared_ptr<NodeDescriptor>>& new_peers) {
    std::vector<std::shared_ptr<NodeDescriptor>> added;
    for (auto new_peer : new_peers) {
        if (_node_lut.find(new_peer->address()) == _node_lut.end()) {
            _view.push_back(new_peer);
            _node_lut[new_peer->address()] = new_peer;
            added.push_back(new_peer);
        }
        else {
            // Reset age of already known peer
            if (_node_lut[new_peer->address()]->age() > new_peer->age()) {
                _node_lut[new_peer->address()]->age() = new_peer->age();
            }
        }
    }
    for (auto& sub : _subscribers) {
        sub->notify_add(added);
    }
}

void URView::manual_insert(std::shared_ptr<NodeDescriptor> new_node) {
    std::lock_guard<std::mutex> _(_lock);
    append(new_node);
}

void URView::manual_insert(std::vector<std::shared_ptr<NodeDescriptor>>& new_nodes) {
    std::lock_guard<std::mutex> _(_lock);
    append(new_nodes);
}

void URView::move_old_to_back(int num_move) {
    if (num_move <= 0 || _view.empty()) {
        // ToDo: Add Logging
        return;
    }
    if (num_move > _view.size()) {
        num_move = _view.size();
    }

    struct GossipNodeAgeComparator {
        bool operator()(const std::shared_ptr<NodeDescriptor>& lhs,
                        const std::shared_ptr<NodeDescriptor>& rhs) const {
            return lhs->age() < rhs->age();  // Older nodes have higher priority
        }
    };

    std::priority_queue<std::shared_ptr<NodeDescriptor>, 
                        std::vector<std::shared_ptr<NodeDescriptor>>, 
                        GossipNodeAgeComparator> age_q;

    std::unordered_map<std::string, int> idx_lut;

    for (int i = 0; i < _view.size(); ++i) {
        age_q.push(_view[i]);
        idx_lut[_view[i]->address()] = i;
    }
    
    int back = _view.size() - 1;

    for (int i = 0; i < num_move && !age_q.empty(); ++i) {
        int oldest_idx = idx_lut[age_q.top()->address()];
        age_q.pop();
        
        if (oldest_idx != back) {
            std::swap(_view[oldest_idx], _view[back]);
            idx_lut[_view[oldest_idx]->address()] = oldest_idx;
        }
        --back;
    }
}

void URView::remove_old(int num_remove) {
    if (num_remove <= 0 || _view.empty()) {
        // ToDo: Add Logging
        return;
    }
    if (num_remove > _view.size()) {
        num_remove = _view.size();
    }
    move_old_to_back(num_remove);
    std::vector<std::string> removed;
    for (int i=0; i < num_remove; ++i) {
        removed.push_back(_view.back()->address());
        _node_lut.erase(_view.back()->address());
        _view.pop_back();
    }
    for (auto& sub : _subscribers) {
        sub->notify_delete(removed);
    }
}

void URView::remove_head(int num_remove) {
    if (num_remove <= 0 || _view.empty()) {
        // ToDo: Add Logging
        return;
    }
    if (num_remove > _view.size()) {
        num_remove = _view.size();
    }
    std::vector<std::string> removed;
    for (int i=0; i < num_remove; ++i) {
        removed.push_back(_view.front()->address());
        _node_lut.erase(_view.front()->address());
    }
    _view.erase(_view.begin(), _view.begin() + num_remove);

    for (auto& sub : _subscribers) {
        sub->notify_delete(removed);
    }
}

void URView::remove_random(int num_remove) {
    if (num_remove <= 0 || _view.empty()) {
        // ToDo: Add Logging
        return;
    }
    if (num_remove > _view.size()) {
        num_remove = _view.size();
    }
    std::vector<std::string> removed;
    for (int i=0; i < num_remove; ++i) {
        std::uniform_int_distribution<> distr(0, _view.size() - 1);
        int rand = distr(_eng);
        removed.push_back(_view[rand]->address());
        _node_lut.erase(_view[rand]->address());
        _view.erase(_view.begin() + rand);
    }

    for (auto& sub : _subscribers) {
        sub->notify_delete(removed);
    }
}

void URView::permute() {
    std::shuffle(_view.begin(), _view.end(), _eng);
}

std::string URView::print() const {
    std::string str = "URView(Self: " + _self->print() 
        + ", Size: " + std::to_string(_size) 
        + ", Healing: " + std::to_string( _healing) 
        + ", Swap: " + std::to_string(_swap)
        + ", Nodes: ";
    for (auto& node : _view) {
        str += node->print() + ", ";
    }
    str += ")";
    return str;
}

std::shared_ptr<View::PeerSelector> URView::create_subscriber(SelectorType type, std::shared_ptr<TSLog> log) {
    std::shared_ptr<View::PeerSelector> sub;

    switch (type) {
        case SelectorType::TAIL:
            sub = std::make_shared<TailPeerSelector>(shared_from_this());
            _subscribers.push_back(sub);
            //std::cout << "Created Gossip Subscriber of type TAIL" << std::endl;
            return sub;

        case SelectorType::LOGGED_TAIL:
            sub = std::make_shared<LoggedTailPeerSelector>(shared_from_this(), log);
            _subscribers.push_back(sub);
            //std::cout << "Created Gossip Subscriber of type LOGGED_TAIL" << std::endl;
            return sub;

        case SelectorType::UNIFORM_RANDOM:
            sub = std::make_shared<URPeerSelector>(shared_from_this());
            _subscribers.push_back(sub);
            //std::cout << "Created Gossip Subscriber of type UNIFORM_RANDOM" << std::endl;
            return sub;

        case SelectorType::LOGGED_UNIFORM_RANDOM:
            sub = std::make_shared<LoggedURPeerSelector>(shared_from_this(), log);
            _subscribers.push_back(sub);
            //std::cout << "Created Gossip Subscriber of type LOGGED_UNIFORM_RANDOM" << std::endl;
            return sub;

        case SelectorType::UNIFORM_RANDOM_NO_REPLACEMENT:
            sub = std::make_shared<URNRPeerSelector>(shared_from_this());
            _subscribers.push_back(sub);
            //std::cout << "Created Gossip Subscriber of type UNIFORM_RANDOM_NO_REPLACEMENT" << std::endl;
            return sub;

        case SelectorType::LOGGED_UNIFORM_RANDOM_NO_REPLACEMENT:
            sub = std::make_shared<LoggedURNRPeerSelector>(shared_from_this(), log);
            _subscribers.push_back(sub);
            //std::cout << "Created Gossip Subscriber of type LOGGED_UNIFORM_RANDOM_NO_REPLACEMENT" << std::endl;
            return sub;

        default:
            std::cout << "Failed to create subscriber because of invalid type" << std::endl;
    }
    return nullptr;
}

}