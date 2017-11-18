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

def make_circle(vertices, list_size, front_end):
     
    leader = 0
    
    for x in range(0, list_size):
        
        if vertices[x] is not None:           
            
            if x%16 < 15:
                
                #make ring
                front_end.add_machine_edge_instance(
                MachineEdge(
                    vertices[x], vertices[x+1],
                    label=(x)), vertices[x].RING)
                
                
                #direct response channel from vertex to leader
                if x != leader:
                    front_end.add_machine_edge_instance(
                        MachineEdge(
                        vertices[x], vertices[leader],
                        label=(x)), vertices[x].REPORT)
                
            #if this is the leader vertex         
            if x%16 == 0:     

                for y in range(1, 16):       
                    front_end.add_machine_edge_instance(
                        MachineEdge(
                        vertices[leader], vertices[leader + y],
                        label=(y-1)), vertices[x].COMMAND)
                
            if x%16 == 15:
                
                #finish ring
                front_end.add_machine_edge_instance(
                MachineEdge(
                    vertices[x], vertices[leader],
                    label=(x)), vertices[x].RING) 
                
                
                front_end.add_machine_edge_instance(
                MachineEdge(
                    vertices[x], vertices[leader],
                    label=(x)), vertices[x].REPORT)         
                
                leader = leader + 16
                           
                                              
                             
           
           