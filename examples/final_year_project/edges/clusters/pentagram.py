'''

def make_pentagram: Builds this cluster within a chip

-----------------------------------------------------------------------------
|                                                                           |
|                                  cluster 1                                |
|                                                                           |
|                                [00]-----[01]                              |
|                                   |     |                                 |
|                                    |   |                                  |
|                                     | |                                   |
|                                   _[10]_                                  |
|                [09]__           _-   |  -_           __[02]               |
|                 |    ---__    _-     |    -_    __---    |                |
|     cluster 5   |       __[14]-----[15]-----[11]__       |   cluster 2    |
|                 |  __---    -_     |  |     _-    ---__  |                |
|                [08]          -_   |    |   _-          [03]               |
|                                - |      | -                               |
|                               [13]------[12]                              |
|                              |  |        |  |                             |
|                             |   |        |   |                            |
|                            |    |        |    |                           |
|                         [07]--[06]      [05]--[04]                        |
|                                                                           |
|                         cluster 4       cluster 3                         |
|                                                                           |
-----------------------------------------------------------------------------

'''
import spinnaker_graph_front_end as front_end

from ..types.pentagram_edge import PentagramEdge

'''-------------------------------------------------------------------------------------------------------------'''

def make_pentagram(vertices, list_size):
    
    first_vertex_index   =  0   #first index in cluster    
    central_vertex_index = 15   #final and central index in cluster   
    counter              =  0   # always goes from  0 to 15
    
    for x in range(0, list_size):
        
        if vertices[x] is not None:
            
            #case 1 build the 5 clusters
            if x%16 < 10:
                
                #x%2 == 0 -> 1,  x%2 == 1 -> -1
                other_cluster_vertex = x + ((x%2)*(-2) + 1)
                
                ring_vertex = (first_vertex_index + 10) + int(counter/2)
                
                #connect to other cluster vertex
                front_end.add_machine_edge(
                    PentagramEdge,
                    {
                        'pre_vertex':  vertices[x],
                        'post_vertex': vertices[other_cluster_vertex],
                        'direction':   PentagramEdge.DIRECTIONS.CLUSTER_CLUSTER
                    },
                    label="ClusterToCluster",
                    semantic_label="TRANSMISSION")
                
                #connect cluster vertex to ring vertex  
                front_end.add_machine_edge(
                    PentagramEdge,
                    {
                        'pre_vertex':  vertices[x],
                        'post_vertex': vertices[ring_vertex],
                        'direction':   PentagramEdge.DIRECTIONS.CLUSTER_RING
                    },
                    label="ClusterToRing",
                    semantic_label="TRANSMISSION")  
                
                #connect ring vertex to cluster vertex  
                front_end.add_machine_edge(
                    PentagramEdge,
                    {
                        'pre_vertex':  vertices[ring_vertex],
                        'post_vertex': vertices[x],
                        'direction':   PentagramEdge.DIRECTIONS.RING_CLUSTER
                    },
                    label="RingToCluster",
                    semantic_label="TRANSMISSION")
                
                counter = counter + 1                                       
                         
            #case 2 and 3: build the central ring and connect the leader
            if x%16 >= 10:
                
                #case 2 building the ring: ...-10-11-12-13-14-10-...
                if x%16 < 15:
                    
                    left_vertex  = x - 1
                    right_vertex = x + 1
                    
                    if left_vertex < (first_vertex_index + 10):
                        left_vertex = first_vertex_index + 14
                        
                    if right_vertex > (first_vertex_index + 14):
                        right_vertex = first_vertex_index + 10                      
                    
                    #connect to left ring vertex
                    front_end.add_machine_edge(
                        PentagramEdge,
                        {
                            'pre_vertex':  vertices[x],
                            'post_vertex': vertices[left_vertex],
                            'direction':   PentagramEdge.DIRECTIONS.RING_LEFT
                        },
                        label="RingToLeft",
                        semantic_label="TRANSMISSION")
                    
                    #connect to right ring vertex
                    front_end.add_machine_edge(
                        PentagramEdge,
                        {
                            'pre_vertex':  vertices[x],
                            'post_vertex': vertices[right_vertex],
                            'direction':   PentagramEdge.DIRECTIONS.RING_RIGHT
                        },
                        label="RingToRight",
                        semantic_label="TRANSMISSION")
                    
                    #connect to central vertex
                    front_end.add_machine_edge(
                        PentagramEdge,
                        {
                            'pre_vertex':  vertices[x],
                            'post_vertex': vertices[central_vertex_index],
                            'direction':   PentagramEdge.DIRECTIONS.RING_CENTRE
                        },
                        label="RingToCentre",
                        semantic_label="TRANSMISSION")
     
                #case 3 central vertex
                if x%16 == 15:
                    
                    #add 5 edges that establish a connection between the centre and the ring cores
                    start = first_vertex_index + 10
                    end   = first_vertex_index + 15
                    
                    for i in range (start, end):
                    
                        front_end.add_machine_edge(
                            PentagramEdge,
                            {
                                'pre_vertex':  vertices[x],
                                'post_vertex': vertices[i],
                                'direction':   PentagramEdge.DIRECTIONS.CENTRE_RING
                            },
                            label="CentreToRing",
                            semantic_label="TRANSMISSION")  
                    
                    #update the indices and reset the counter - moving on to next chip
                    first_vertex_index   = x + 1  
                    central_vertex_index = first_vertex_index + 15
                    counter = 0      
                           
'''-------------------------------------------------------------------------------------------------------------'''
           