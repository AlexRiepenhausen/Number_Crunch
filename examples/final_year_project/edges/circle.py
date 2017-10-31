'''
def make_circle: Builds this cluster within a chip

------------------------------------
|                                  |
|   [00]--[01]--[02]--[03]--[04]   |
|    |                        |    |
|   [15]                    [05]   |
|    |                        |    |
|   [14]                    [06]   |
|    |                        |    |
|   [13]                    [07]   |
|    |                        |    |
|   [12]--[11]--[10]--[09]--[08]   |
|                                  |
------------------------------------

'''

from circle_edge import CircleEdge

def make_circle(vertices, list_size, front_end):
    
    first_vertex_index   =  0   #first index in cluster    
    central_vertex_index = 15   #final and central index in cluster   
    
    for x in range(0, list_size):
        
        if vertices[x] is not None:
            
            #before you reach the final core
            if x%16 < 15:
            
                #connect to other cluster vertex
                front_end.add_machine_edge(
                    CircleEdge,
                    {
                        'pre_vertex':  vertices[x],
                        'post_vertex': vertices[x+1],
                        'direction':   CircleEdge.DIRECTIONS.NEXT
                    },
                    label="ClusterToCluster",
                    semantic_label="TRANSMISSION")
            
            #close the circle
            if x%16 == 15: 
                
                front_end.add_machine_edge(
                    CircleEdge,
                    {
                        'pre_vertex':  vertices[x],
                        'post_vertex': vertices[first_vertex_index],
                        'direction':   CircleEdge.DIRECTIONS.NEXT
                    },
                    label="ClusterToCluster",
                    semantic_label="TRANSMISSION")
                
                first_vertex_index  =  central_vertex_index +  1
                central_vertex_index = first_vertex_index   + 15              
           
           