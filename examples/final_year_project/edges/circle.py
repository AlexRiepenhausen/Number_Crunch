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

from pacman.model.graphs.machine import MachineEdge

def make_circle(vertices, num_vertices, front_end):
     
    leader = 0
    
    for i in range(0, num_vertices):
        
        if vertices[i] is not None:           
            
            if i%16 < 15:
                
                #make ring
                front_end.add_machine_edge_instance(
                MachineEdge(
                    vertices[i], vertices[i+1],
                    label=(i)), vertices[i].RING)
                
                
                #direct response channel from vertex to leader
                if i != leader:
                    front_end.add_machine_edge_instance(
                        MachineEdge(
                        vertices[i], vertices[leader],
                        label=(i)), vertices[i].REPORT)
                
            #if this is the leader vertex         
            if i%16 == 0:     

                for j in range(1, 16):       
                    front_end.add_machine_edge_instance(
                        MachineEdge(
                        vertices[leader], vertices[leader + j],
                        label=(j-1)), vertices[i].COMMAND)
            
            #if this is the last vertex in the cluster  
            if i%16 == 15:
                
                #finish ring
                front_end.add_machine_edge_instance(
                MachineEdge(
                    vertices[i], vertices[leader],
                    label=(i)), vertices[i].RING) 
                
                front_end.add_machine_edge_instance(
                MachineEdge(
                    vertices[i], vertices[leader],
                    label=(i)), vertices[i].REPORT)         
                
                leader = leader + 16
                           
                                              
                             
           
           