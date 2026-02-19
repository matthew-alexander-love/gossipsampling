# Gossip Peer Sampling Service (GPSS)

A Gossip Peer Sampling Service (GPSS) creates a dynamic, random, self-healing overlay network and provides peers for communication within that topology. This library is designed to be a general-purpose implementation of GPSS, making it useful for a variety of large-scale distributed systems.

Check out a walk through and tutorial [here](https://bytesandbrains.ai/gossip-peer-sampling-service/)

## Features

- General-Purpose Library: Build on a flexible GPSS implementation inspired by Epidemic-style Dissemination in Large-Scale Systems.
- Extensibility: Supports the integration of custom algorithms for experimentation and deployment.
- Multi-Language Support: C++ Core Library, Python Simulation Software, and HighLevel API in both languages
- gRPC Integration: Leverage gRPC for efficient communication in large-scale distributed systems.
- Multiple Peer Selection Subscription that are independent from one another.
- Open Source: This project is licensed under the terms of the GPLv3 license. Contributions are welcome to improve functionality or add new features. Check out the Future Features section to see how you could help.

#### Future Ideas
- Connection Security (mTLS)
- Monitored and configurable bootstrap servers
- Dashboards with network statistics
- Enhanced simulation capability
- Remote Peer Sampling mode

## Getting Started:

#### Building for C++
- You can either have a system wide installation (see [gRPC Installation Guide](https://grpc.io/docs/languages/cpp/quickstart/)) or you can have a project local installation.
    - System Wide gRPC Install Command: 
        - `cmake -S . -B cbuild -DINSTALL_LOCALLY=OFF`
    - Project Local gRPC Install Command:
        - `cmake -S . -B cbuild -DINSTALL_LOCALLY=ON`
- Build the project:
    - `cmake --build cbuild `
- Now the library should be good to use. Simply link against it in your build system and add the `#include <gossip>` header.
- Alternatively you can set the configurations manually in `CMakeLists.txt` and `add_subdirectory(*proj_dir*)` to your own CMake based project. 


#### Building for Python
- pip install pybind11 setuptools
- pip install .

## Examples

Once the project has been built, there are examples for both Python and C++ located in the `example/` directory. The behavior is the same for both languages:

### `gossip_entry_server_example.*`
- **Description**: Starts a gossip entry server. The node's address should be passed as an argument. Defaults to `192.168.1.173:50000` if no argument is provided.
- **Usage**:
  - **C++**:
    ```bash
    # IPv4
    cd cbuild
    ./gossip_entry_server_example 0.0.0.0:50000
    ```
    ```bash
    # IPv6
    cd cbuild
    ./gossip_entry_server_example [::]:50000
    ```
  - **Python**:
    ```bash
    # IPv4
    cd example
    python gossip_entry_server_example.py 0.0.0.0:50000
    ```
    ```bash
    # IPv6
    cd example
    python gossip_entry_server_example.py [::]:50000
    ```

### `gossip_client_example.*`
- **Description**: Starts a gossip client. 
  - The first parameter is the node's address. Defaults to `192.168.1.173:60000` if not provided.
  - The subsequent parameters are the addresses of entry servers. Defaults to `192.168.1.173:50000` if no arguments are provided.
- **Usage**:
  - **C++**:
    ```bash
    # IPv4
    cd cbuild
    ./gossip_client_example 0.0.0.0:60000 0.0.0.0:50000
    ```
    ```bash
    # IPv6
    cd cbuild
    ./gossip_client_example [::]:60000 [::]:50000
    ```
  - **Python**:
    ```bash
    # IPv4
    cd example
    python gossip_client_example.py 0.0.0.0:60000 0.0.0.0:50000
    ```
    ```bash
    # IPv6
    cd example
    python gossip_client_example.py [::]:60000 [::]:50000
    ```

### Emulating a Network over LAN
To emulate a network over a local area network (LAN), you can:
1. Start an entry server.
2. Run multiple instances of the client, ensuring that each client is assigned a unique port number.

### Network Simulation in Python
Follow the notebook /example/python/simulation_example.ipynb that runs network simulations, calculates statistics, and compares them to the original paper.

<br><br><br>

# Simulation Documentation

Only documentation for the simulator is recorded as using the main library is quite simple unless you want to implement custom views or entry and exit protocols. At which point your best bet is just looking at the codebase anyways (start with view.h and peer_samping_service.h).

## NodeSchema

Defines a schema for creating simulation nodes with configurable parameters.

### Parameters
- `name` (str): Name of the type this schema defines
- `push` (bool): Enable push-based communication
- `pull` (bool): Enable pull-based communication
- `wait_time` (int): Time to wait between communication attempts
- `timeout` (int): Timeout for communication
- `func` (Callable[[gossip.PeerSamplingService, gossip.View, gossip.TSLog, threading.Event], None]): Function to be executed by the node's thread 
- `view_type` (gossip.View): Type of view to use for network topology
- `selector_type` (gossip.SelectorType): Type of selector for view management
- `**view_kargs`: Additional arguments for view initialization

### Methods
`gen_node(address, entry_times, exit_times, entry_points=[])`
Generates a SimNode based on the schema configuration.
Parameters
- `address` (str): Network address for the node
- `entry_times` (dict[str, int]): Dictionary to track node entry times
- `exit_times` (dict[str, int]): Dictionary to track node exit times
- `entry_times` (list[str], optional): Initial entry points for the node
Returns
- `SimNode` A newly created node with the specified configuration

### Example Usage
The `gen_node()` method is generally only called with the `Simulator` object
```python
schema = NodeSchema(
    name="GossipNode",
    push=True,
    pull=True,
    wait_time=1,
    timeout=5,
    func=custom_node_function,
    view_type=NetworkView,
    selector_type=RandomSelector,
    size=20, 
    healing=0,
    swap=10 
)
```
<br>

## Simulator

The `Simulator` class manages and executes simulations involving multiple network nodes, events, and data logging. It integrates node schemas, handles node lifecycle events, and supports configurable event-driven simulations

### Parameters
- `schema` (dict[str, NodeSchema]): Registry of node schemas defining the characteristics of various node types.
- `events` (list[TopologyConstructor]): List of events to construct and execute during the simulation.
- `log_dir` (Path, optional): Directory to store simulation logs. Defaults to sim_logs/
- `name` (str, optional): Name of the simulation used to save. If None, it defaults to the simulation's start time.

### Methods
There are many internal methods, if not for being in Python all but the below would be 'private'
`run(stats_interval: int = -1, save: bool = True) -> None`
- `stats_interval` (int, optional): Interval for calculating statistics. Defaults to -1. IF -1, indicates not to calc node and network statistics
- `save` (bool, optional): Whether to save logs and statistics. Defaults to True.

### Example Usage
```python
schema_registry = {
    "GossipNode": gossip.NodeSchema(
        name="GossipNode",
        push=True,
        pull=True,
        wait_time=1,
        timeout=5,
        func=custom_node_function,
        view_type=gossip.URView,
        selector_type=gossip.SelectorType.TAIL,
        view_args={'max_size': 10}
    )
}

# Define events
events=[gossip.AddEntryServers(entry_nodes={"GossipNode": 4}),
        gossip.AddRate(run_time=100, add_rates={"GossipNode": (10, 1)}),
        gossip.AddDelay(delay_time=10),
        gossip.ChurnRate(run_time=1000, add_rates={"GossipNode": (3, 1)}, rem_rates={"GossipNode": (3, 1)})
]

# Initialize the simulator
sim = Simulator(schema=schema_registry, events=events, name="ExampleSimulation")

# Run the simulation
sim.run(stats_interval=2000)
```
<br>

## Simulation Topology Constructors

These topology constructors allow you to dynamically modify the network topology during a simulation by adding, removing, or delaying nodes.

<br>

## RemoveRate

Removes nodes from the simulation at specified rates.

### Parameters
- `run_time` (int): Total duration of the node removal process
- `rem_rates` (dict[str, tuple[int, int]], optional): Rates for removing nodes
  - Key: Node type
  - Value: Tuple of (remove_x_nodes, per_y_seconds >= 1)
- `entry_rem_rates` (dict[str, tuple[int, int]], optional): Rates for removing entry nodes
  - Key: Entry node type
  - Value: Tuple of (remove_x_entry_nodes, per_y_seconds >= 1)

### Example Usage
```python
remove_config = RemoveRate(
    run_time=60,
    rem_rates={'Echo': (4, 1), 'Database': (1, 2)},  # Remove 4 Echo Type Nodes per 1 second and 1 Database Type Node per 2 seconds
    entry_rem_rates={'BootStrap': (2, 1)}  # Remove 2 Bootstrap Type Entry Nodes per 1 second
)
```

<br>

## ChurnRate

Allows both adding and removing nodes during the simulation.

### Parameters
- `run_time` (int): Total duration of defined network churn
- `rem_rates` (dict[str, tuple[int, int]], optional): Rates for removing nodes
  - Key: Node type
  - Value: Tuple of (removal start time, removal end time)
- `entry_rem_rates` (dict[str, tuple[int, int]], optional): Rates for removing entry nodes
  - Key: Entry node type
  - Value: Tuple of (removal start time, removal end time)
- `add_rates` (dict[str, tuple[int, int]], optional): Rates for adding nodes
    - Key: Node type
    - Value: Tuple of (add_x_nodes, per_y_seconds >= 1)
- `entry_add_rates` (dict[str, tuple[int, int]], optional): Rates for adding entry nodes
    - Key: Entry node type
    - Value: Tuple of (add_x_entry_nodes, per_y_seconds >= 1)

### Example Usage
```python
churn_config = ChurnRate(
    run_time=120,
    rem_rates={'Echo': (2, 1), 'Database': (1, 2)},  # Remove 2 Echo Nodes per 1 second, 1 Database Node per 2 seconds
    add_rates={'Worker': (3, 1)},  # Add 3 Worker Nodes per 1 second
    entry_rem_rates={'BootStrap': (1, 1)},  # Remove 1 Bootstrap Entry Node per 1 second
    entry_add_rates={'Aggregator': (2, 1)}  # Add 2 Aggregator Entry Nodes per 1 second
)
```

<br>

## AddRate

Allows adding nodes during the simulation.

### Parameters
- `run_time` (int): Total duration to add nodes
- `add_rates` (dict[str, tuple[int, int]], optional): Rates for adding nodes
    - Key: Node type
    - Value: Tuple of (add_x_nodes, per_y_seconds >= 1)
- `entry_add_rates` (dict[str, tuple[int, int]], optional): Rates for adding entry nodes
    - Key: Entry node type
    - Value: Tuple of (add_x_entry_nodes, per_y_seconds >= 1)

### Example Usage
```python
churn_config = ChurnRate(
    run_time=120,
    add_rates={'Worker': (3, 1)},  # Add 3 Worker Nodes per 1 second
    entry_add_rates={'Aggregator': (2, 1)}  # Add 2 Aggregator Entry Nodes per 1 second
)
```

<br>

## AddDelay

Introduces a delay in the simulation. Use to allow existing network to run without interference.

### Parameters
- `delay_time` (int): Total duration of delay

### Example Usage
```python
churn_config = ChurnRate(
    delay_time=120,
)
```

<br>

## AddLattice (Ring)

Creates a lattice topology by adding nodes with connections predefined upon entry.

### Parameters
- `entry_nodes` (dict[str, int], optional): Number of entry nodes of each type to add
    - Key: Node type
    - Value: Number of entry nodes to add
- `nodes` (dict[str, int], optional): Number of nodes of each type to add
    - Key: Entry node type
    - Value: Number of nodes to add
- `use_entry`: Should these nodes still use entry_servers after entering with there initial lattice topology

<br>

### Example Usage
```python
lattice_config = AddLattice(
    entry_nodes={'BootStrap': 2},  # Add 2 Bootstrap Entry Nodes
    nodes={'Worker': 5, 'Database': 2}  # Add 5 Worker Nodes and 2 Database Nodes
)
```

<br>

## AddEntryServers

Adds entry nodes to the simulation.

### Parameters
- `entry_nodes` (dict[str, int], optional): Number of entry nodes of each type to add
    - Key: Node type
    - Value: Number of entry nodes to add

### Example Usage
```python
entry_config = AddEntryServers(
    entry_nodes={'Client': 3, 'BootStrap': 2}  # Add 3 Client Entry Nodes and 2 Bootstrap Entry Nodes
)
```

<br>

## AddErdosRenyi

Creates a random graph topology using the Erdős-Rényi model

### Parameters
- `entry_nodes` (dict[str, int], optional): Number of entry nodes of each type to add
    - Key: Node type
    - Value: Number of entry nodes to add
- `nodes` (dict[str, int]): Number of nodes of each type to add
    - Key: Node type
    - Value: Number of nodes to add
- `edge_prob` (float): Probability of an edge existing between any two nodes

### Example Usage
```python
erdos_config = AddErdosRenyi(
    entry_nodes={'BootStrap': 2},  # Add 2 Bootstrap Entry Nodes
    nodes={'Worker': 5, 'Database': 2},  # Add 5 Worker Nodes and 2 Database Nodes
    edge_prob=0.3  # 30% chance of an edge between any two nodes
)
```

## AddUniformRandom

Creates a random graph topology using the Erdős-Rényi model

### Parameters
- `entry_nodes` (dict[str, int], optional): Number of entry nodes of each type to add
    - Key: Node type
    - Value: Number of entry nodes to add
- `nodes` (dict[str, int]): Number of nodes of each type to add
    - Key: Node type
    - Value: Number of nodes to add
- `edge_prob` (float): Probability of an edge existing between any two nodes

### Example Usage
```python
erdos_config = AddErdosRenyi(
    entry_nodes={'BootStrap': 2},  # Add 2 Bootstrap Entry Nodes
    nodes={'Worker': 5, 'Database': 2},  # Add 5 Worker Nodes and 2 Database Nodes
    edge_prob=0.3  # 30% chance of an edge between any two nodes
)
```
<br><br>


## Notice

This product uses gRPC which is licensed under the [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)
