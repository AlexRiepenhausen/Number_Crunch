import os

import spinnaker_graph_front_end as front_end
from utilities.parser import parser
from data_input.build_vertices import load_data_onto_vertices
from data_output.output_to_console import output_to_console
from spinnman import model_binaries

#put aplx file into: /home/mfbx3ard/.local/lib/python2.7/site-packages/spinnman/model_binaries
front_end.setup(
    n_chips_required=None, model_binary_folder=os.path.dirname(model_binaries.__file__))

total_number_of_cores = \
    front_end.get_number_of_available_cores_on_machine()
    
getData = parser('../../resources/test.csv')    
    
data = getData.read_data()
number_of_chips = 1
columns = [0]
num_string_cols = 1
function_id = 2 

load_data_onto_vertices(data,
                        number_of_chips,
                        columns,
                        num_string_cols,
                        function_id)

front_end.run(10000)

placements = front_end.placements()
buffer_manager = front_end.buffer_manager()

results = output_to_console(placements, buffer_manager)
results.function_three()

front_end.stop()
