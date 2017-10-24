from enum import Enum
from pacman.model.graphs.machine import MachineEdge

'''used in partitioning 18 cores of each chip into separate processing regions'''

class PentagramEdge(MachineEdge):

    DIRECTIONS = Enum(value="EDGES",
                      names=[("CLUSTER_CLUSTER", 0),
                             ("RING_CLUSTER", 1),
                             ("CLUSTER_RING", 2),
                             ("RING_LEFT", 3),
                             ("RING_RIGHT", 4),
                             ("RING_CENTRE", 5),
                             ("CENTRE_RING", 6)])

    def __init__(self, pre_vertex, post_vertex, direction, n_keys=1,
                 label=None):
        MachineEdge.__init__(
            self, pre_vertex, post_vertex, label=label)
        self._direction = direction

    @property
    def direction(self):
        return self._direction

    def __str__(self):
        return self.__repr__()

    def __repr__(self):
        return "Edge:{}:{}".format(self.label, self._direction)
