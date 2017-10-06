
"""
Main file for executing the application
"""

from parser import parser
import spinnaker_graph_front_end as front_end
import logging
import os

"""
from spinnaker_graph_front_end.examples.final_year_project.histogram_vertex\
    import HistogramVertex
"""


#read the csv data with help form the parser class
getData = parser('../../resources/test.csv')
raw_data = getData.read_data()


logger = logging.getLogger(__name__)

front_end.setup(
    n_chips_required=None, model_binary_folder=os.path.dirname(__file__))

# calculate total number of 'free' cores for the given board
# (i.e. does not include those busy with SARK or reinjection)
total_number_of_cores = \
    front_end.get_number_of_available_cores_on_machine()

# fill all cores with HistogramVertex each
for x in range(0, total_number_of_cores):
    
    """
    front_end.add_machine_vertex(
        HistogramVertex,
        {},
        label="HistogramVertex at x {}".format(x))
    """

front_end.run(10)

placements = front_end.placements()
buffer_manager = front_end.buffer_manager()

'''
for placement in sorted(placements.placements,
                        key=lambda p: (p.x, p.y, p.p)):

    Read the output from the SpiNNaker machine and get a data histogram
'''

front_end.stop()
    