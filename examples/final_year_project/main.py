
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

from parser import parser
from vertex import Vertex

import spinnaker_graph_front_end as front_end
import logging
import os

'''-----------------------------------------------------------------------------------------------------'''

def load_data_onto_spinnaker(total_number_of_cores, data):
    
    data_len = len(data) -1
    
    for x in range(0, total_number_of_cores):
        if x < data_len:
            front_end.add_machine_vertex(
                 Vertex,
                 {
                  "entry": [data[x][0],data[x][0]],
                  "value": [123]
                 },
            label="Data packet at x {}".format(x))   
        
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

load_data_onto_spinnaker(total_number_of_cores, raw_data)

front_end.run(2)

placements = front_end.placements()
buffer_manager = front_end.buffer_manager()

for placement in sorted(placements.placements,
                        key=lambda p: (p.x, p.y, p.p)):

    if isinstance(placement.vertex, Vertex):
        result = placement.vertex.read(placement, buffer_manager)
        logger.info("{}, {}, {} > {}".format(
            placement.x, placement.y, placement.p, result))

front_end.stop()
