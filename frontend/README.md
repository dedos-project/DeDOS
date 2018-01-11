Authors: Bowen Wang, Isaac Pedisich
Last Updated: Dec-21-2017

## Installation:
```shell
$ sudo apt-get install nodejs-legacy
```

## Configuration
In `config.js`:

1. Set `config.ssh_key` to the path to a local private key which
will allow this program to have passwordless access to remote nodes mentioned in the DFG.
2. Set `config.user_name` to the corresponding user for the ssh key
3. Set `config.ssh_port` to the port used to connect to the nodes over ssh

## DFG population
Place DFGs to run in the directory specified in `config.local_dfg_dir`

## Running
From inside dedos-frontend directory, run `node server`
