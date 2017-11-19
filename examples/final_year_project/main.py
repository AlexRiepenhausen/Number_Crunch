
"""
Main file for executing the application
"""

'''
source ~/git/spinnaker_tools/setup 
Compile commands:
export SPINN_DIRS=~/git/spinnaker_tools
export PATH=$PATH:~/gcc-arm-none-eabi-5_4-2016q3/bin
make 
'''

'''-----------------------------------------------------------------------------------------------------'''

from edges.circle import make_circle
from utilities.parser import parser
from vertex import Vertex 

import spinnaker_graph_front_end as front_end
import logging
import os

'''-----------------------------------------------------------------------------------------------------'''

def display_results_function_one():
    
        for placement in sorted(placements.placements,
            key=lambda p: (p.x, p.y, p.p)):

            if isinstance(placement.vertex, Vertex):
                result = placement.vertex.read(placement, buffer_manager)
                logger.info("{}, {}, {} > {}".format(
                placement.x, placement.y, placement.p, result))
    

def display_results_function_two():

        for placement in sorted(placements.placements,
            key=lambda p: (p.x, p.y, p.p)):

            if isinstance(placement.vertex, Vertex):
        
                result = placement.vertex.read(placement, buffer_manager)
         
                logger.info("|----------------|----|") 
                logger.info("| Core {}, {}, {}".format(placement.x, placement.y, placement.p))   
                logger.info("|----------------|----|") 
        
            for x in range(0, 4):
                start = 0  + 16*x;
                end   = 15 + 16*x;
                logger.info("| {}| {}".format(result[start:end],result[start+64:end+64]))

'''-----------------------------------------------------------------------------------------------------'''

def load_data_onto_vertices(total_number_of_cores, data):
    
    #get rid of the headers
    del data[0]

    vertices = []
    for x in range(0, 16):
            
        #initiate if this is the first vertex in the circle
        initiate = 0
        if x%16 == 0:
            initiate = 1
            
        data_parcel = [data[x+ 0][0],data[x+ 0][1]], \
                      [data[x+16][0],data[x+16][1]], \
                      [data[x+32][0],data[x+32][1]], \
                      [data[x+48][0],data[x+48][1]]
                                 
        current_vertex = front_end.add_machine_vertex(
            Vertex,
            {
            "columns":         2,
            "rows":            4,
            "string_size":     16,
            "num_string_cols": 1,
            "entries":         data_parcel,
            "initiate":        initiate,
            "function_id":     2,
            "state":           x
            },
            label="Data packet at x {}".format(x))   
           
        vertices.append(current_vertex)   
            
    make_circle(vertices, len(vertices), front_end)
        
'''-----------------------------------------------------------------------------------------------------'''
        
#read the csv data with help form the parser class
getData = parser('../../resources/test.csv')
raw_data = getData.read_data()

logger = logging.getLogger(__name__)

front_end.setup(
    n_chips_required=None, model_binary_folder=os.path.dirname(__file__))

'''
calculate total number of 'free' cores for the given board
(i.e. does not include those busy with SARK or reinjection)'''
total_number_of_cores = \
    front_end.get_number_of_available_cores_on_machine()

'''
determine the data volume each core should take'''
total_number_of_items = len(raw_data) - 1
volume_per_core = total_number_of_items/total_number_of_cores

load_data_onto_vertices(total_number_of_cores, raw_data)

front_end.run(300)

placements = front_end.placements()
buffer_manager = front_end.buffer_manager()

display_results_function_two()

front_end.stop()
