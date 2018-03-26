# TETRiS

## Build

Use the following commands to build TETRiS

```bash
mkdir build && cd build
cmake ..
make
```


## Run

To be able to manage an application with TETRiS two steps are necessary.
First of all you need to start the TETRiS-server and give it the path
to the mappings for each program. The server executable is located in the
'bin'-directory.

After the server started up, it is now possible to run other applications and
let the TETRiS server manage them. To achieve this, one has to add the
TETRiS-client library to the program. This can be achieved by preloading the
library with the LD_PRELOAD primitive. The library is located in the 
'lib'-directory.

## Settings

### Server

See the help message of the server for information about how the server can be
tweaked.

In addition the server also parses the following environment variables:

#### TETRIS_LOG_LEVEL

With this environment variable you can control how much information the TETRiS
server outputs. Possible values are DEBUG, INFO, WARNING and ERROR.


### Client

The client reacts to multiple environment variables:

#### TETRIS_LOG_LEVEL

With this environment variable you can control how much information the TETRiS
client library outputs. Possible values are DEBUG, INFO, WARNING and ERROR.

#### TETRIS_MAPPING_TYPE

This environment variable will tell TETRiS in which way the clients should be pinned
to the associated CPUs. If this variable is set to 'DYNAMIC', no pinning should be
used for the client threads but instead the threads should be left movable. Only the
list of available CPUs will be limited to the ones that the selected mapping contains.
If this variable is not set or the value 'STATIC' is used, TETRiS will pin the client
threads to the exact CPUs as defined in the selected mapping.

#### TETRIS_COMPARE_CRITERIA

With this environment variable one can choose which characteristic of a mapping should
be compared when searching for the best possible mapping on the server. This option can
be changed per application that is executed. If omitted, execution time will be used as
comparison characteristic.

#### TETRIS_COMPARE_MORE_IS_BETTER

If not set, the TETRiS server will assume that smaller values for the mapping characteristic
are better than larger ones. Hence the server will always prefer the mapping for the
application with the smallest value in the compare characteristic. With this environment
variable one can change this behavior to more is better.

#### TETRIS_FILTER_CRITERIA

With this environment variable you can influence which mappings available for the TETRiS server
are actually considered for the application. Hence, one can filter out mappings that don't match
the given criteria. The syntax to define the filter is as follows:

    {mapping characteristic}{compare operator}{value}

Valid examples can look like the following:

    energyConsumption<2000          or
    totalExecutionTime<=100

Spaces at the beginning, end and around the compare operator are striped away. Currently supported
compare operators are: '<', '<=', '>', '>=', '==', '=', '!='

The given filter criteria is a positive criteria. This means, that only mappings that fulfill this
criteria are considered for the application.

#### TETRIS_PREFERRED_MAPPING

This environmental variable can be used to force the server to use one particular
mapping. The value of this variable should be the name of the preferred mapping.


## Control Interface

### Signals

In addition to only managing the various clients that connect to the TETRiS server, the server also
reacts to various signals that are sent to it. At the moment, the following reactions exist:

#### SIGUSR1

Upon a SIGUSR1 signal, the TETRiS server will reload its mapping database. Accordingly, if mappings
change in the meantime (new applications are installed, …), the TETRiS server will now know about
them. However, be aware that already running and managed applications are not remapped according to
the new mapping database.

#### SIGUSR2

Upon a SIGUSR2 signal, the TETRiS server will output information about the applications that it
currently manages. This information contains which mapping is currently used for the client and
where its threads are currently mapped.

### tetrisctl

tetrisctl is an additional binary that can be used to send various commands to the TETRiS server.
See the tetrisctl binary help for more information about which commands are supported.
