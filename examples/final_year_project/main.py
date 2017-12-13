
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
from vertex import Vertex 
from utilities.parser import parser

import spinnaker_graph_front_end as front_end
import logging
import os
import math

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
            
def display_results_function_three():
    
    total = 0  
    
    logger.info("|------------------|----------|")
    
    for placement in sorted(placements.placements,
        key=lambda p: (p.x, p.y, p.p)):

        if isinstance(placement.vertex, Vertex):

            result = placement.vertex.read(placement, buffer_manager)
                                
            number_of_entries = len(result)/26
                   
            for entry in range(0,number_of_entries):   
                            
                string_start = 26*entry +  0;    
                string_end   = 26*entry + 16;
                int_start    = 26*entry + 16;
                int_end      = 26*entry + 26;   
                
                temp1 = []
                for x in range (int_start,int_end):
                    if result[x] != 0:        
                        temp1.append(result[x])
                    else:
                        break                         

                total = total + int(''.join(chr(i) for i in temp1))

                logger.info("| {} | {}".format(result[string_start:string_end], \
                                               result[int_start:int_end]))
                
            logger.info("                               ")
         
    logger.info("|------------------|----------|")
    logger.info("| Total            | %d",  total)
    logger.info("|------------------|----------|")
                            
def write_unique_ids_to_csv(getData,number_of_chips,num_data_rows):
    
    num_processors = number_of_chips * 16
    rows_per_core = int(math.floor(num_data_rows/num_processors))
    
    leftovers = num_data_rows % num_processors
    core = 0
    
    id_array = []
    
    for placement in sorted(placements.placements,
        key=lambda p: (p.x, p.y, p.p)):

        if isinstance(placement.vertex, Vertex):
            result = placement.vertex.read(placement, buffer_manager)
        
        add_left_over = 0
        if core < leftovers:
            add_left_over = 1

        for x in range(0, rows_per_core + add_left_over):
            start = 0   + 10*x;
            end   = 9   + 10*x;
            id_array.append(''.join(chr(i) for i in result[start:end]))
            #print ''.join(chr(i) for i in result[start:end])
            
        core = core + 1
        
    getData.write_to_csv('../../resources/output.csv', id_array)
    
def display_linked_list_size():
    
    for placement in sorted(placements.placements,
        key=lambda p: (p.x, p.y, p.p)):

        if isinstance(placement.vertex, Vertex):
        
            result = placement.vertex.read(placement, buffer_manager)
            
            temp1 = []
            for x in range (0,10):
                if result[x] != 0:        
                    temp1.append(result[x])
                else:
                    break
            
            temp2 = []
            for x in range (10,20):
                if result[x] != 0:        
                    temp2.append(result[x])
                else:
                    break
                
            rows   = int(''.join(chr(i) for i in temp1))
            length = int(''.join(chr(i) for i in temp2))
         
            logger.info("|----------------|") 
            logger.info("| Core {}, {}, {}".format(placement.x, placement.y, placement.p))   
            logger.info("| Rows %d",rows)
            logger.info("| List %d",length)
            logger.info("| TCM Memory for rows: %d bytes", (rows * 2))
            logger.info("| TCM Memory for list: %d bytes", (length * 40))
            logger.info("| TCM Memory total   : %d bytes", (rows * 2 + length * 40))
'''-----------------------------------------------------------------------------------------------------'''

def load_data_onto_vertices(data, number_of_chips, columns, num_string_cols, function_id):
    
    #get rid of the headers
    del data[0]
    
    num_processors = number_of_chips * 16
    num_data_rows  = len(data)
    
    rows_per_core = int(math.floor(num_data_rows/num_processors))
    
    leftovers = num_data_rows % num_processors
    
    row_count = 0

    vertices = []
    for core in range(0, num_processors):
            
        #initiate if this is the first vertex in the circle
        initiate = 0
        if core%16 == 0:
            initiate = 1
          
        #distribute the data evenly among the cores
        add_leftover = 0
        if core < leftovers:
            add_leftover = 1
            
        data_parcel = [] 
        
        for row in range(0, rows_per_core + add_leftover):
            
            data_row = []  
            for z in range(0, len(columns)):
                data_row.append(data[row_count][columns[z]])
            
            row_count = row_count + 1  
            data_parcel.append(data_row)
            
        #load information onto the vertex             
        current_vertex = front_end.add_machine_vertex(
            Vertex,
            {
            "columns":         len(columns),
            "rows":            rows_per_core + add_leftover,
            "string_size":     16,
            "num_string_cols": num_string_cols,
            "entries":         data_parcel,
            "initiate":        initiate,
            "function_id":     function_id,
            "state":           core
            },
            label="Data packet at x {}".format(core))   
           
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

#param1: data
#param2: number of chips used
#param3: what columns to use
#param4: how many string columns exist?
#param5: function id
load_data_onto_vertices(raw_data, 1, [0], 1, 2)

front_end.run(10000)

placements = front_end.placements()
buffer_manager = front_end.buffer_manager()

#write_unique_ids_to_csv(getData,1,len(raw_data))
#display_linked_list_size()
#display_results_function_one()
#display_results_function_two()
display_results_function_three()
front_end.stop()
