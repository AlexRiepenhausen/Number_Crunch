from utilities.parser import parser

import os
import spinnaker_graph_front_end as front_end

from examples.final_year_project.output import output_to_console
from examples.final_year_project.input  import build_vertices

front_end.setup(
    n_chips_required=None, model_binary_folder=os.path.dirname(__file__))

total_number_of_cores = \
    front_end.get_number_of_available_cores_on_machine()
    
getData = parser('../../resources/date.csv')    
    
data            = getData.read_data()
number_of_chips = 1
columns         = [0]
num_string_cols = 1
function_id     = 3

front_end.run(10000)

placements     = front_end.placements()
buffer_manager = front_end.buffer_manager()
output = output_to_console(placements, buffer_manager)
output.display_results_function_three()

front_end.stop()
