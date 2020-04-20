letce2-tutorial
==

# Introduction

The Lightweight Experiment Template Configuration Environment
([letce2][1]) provides a hierarchical mechanism for generating
experiment configuration using the [Mako][2] template engine.

[1]: https://github.com/adjacentlink/letce2
[2]: https://https://www.makotemplates.org

[me @host] cd exp-foo
[me@host exp-foo]$ make
letce2 \
	lxc \
	build \
	../node.cfg.d/foo.cfg \
	experiment.cfg
```

The `letce2 lxc build` command, which is used by the make all rule,
takes as input all the configuration files making up the experiment
definition. For exp-foo, `node.cfg.d/foo.cfg` contains
the configuration definition for an foo radio node and
`experiment.cfg` contains server (host) configuration, configuration
that is common to all nodes, and the declaration and configuration of
each node in the experiment.

The output of `letce2 lxc build` is a directory for every node defined
in the experiment. For 10 nodes, this results in 10 node directories:
`node-1` through `node-10`, and one server directory: `host`.

# Experiment Start and Stop

An experiment can be run with the `letce2 lxc start` command:

```
[me@host] letce2 lxc start
```

Similarly an experiment can be stopped with the `letce2 lxc stop`
command:

```
[me@host] letce2 lxc stop
```
