
from pacman.model.decorators import overrides
from pacman.model.graphs.machine import MachineVertex
from pacman.model.resources import CPUCyclesPerTickResource, DTCMResource
from pacman.model.resources import ResourceContainer, SDRAMResource
from pacman.utilities import utility_calls

from spinn_front_end_common.utilities import globals_variables
from spinn_front_end_common.utilities import constants, helpful_functions, exceptions
from spinn_front_end_common.interface.simulation import simulation_utilities
from spinn_front_end_common.abstract_models.impl \
    import MachineDataSpecableVertex
from spinn_front_end_common.abstract_models import AbstractHasAssociatedBinary
from spinn_front_end_common.interface.buffer_management.buffer_models\
    import AbstractReceiveBuffersToHost
from spinn_front_end_common.interface.buffer_management\
    import recording_utilities
from spinn_front_end_common.utilities.utility_objs import ExecutableStartType

from enum import Enum
import logging

logger = logging.getLogger(__name__)

from utilities.string_marshalling import _32intarray_to_int
from utilities.string_marshalling import convert_string_to_integer_parcel

class Vertex(
        MachineVertex, MachineDataSpecableVertex, AbstractHasAssociatedBinary,
        AbstractReceiveBuffersToHost):
    
    PARTITION_ID = "STATE"

    TRANSMISSION_DATA_SIZE = 2 * 4  # has key and key
    STATE_DATA_SIZE = 1 * 4  # 1 or 2 based off dead or alive
    NEIGHBOUR_INITIAL_STATES_SIZE = 2 * 4  # alive states, dead states

    # Regions for populations
    DATA_REGIONS = Enum(
        value="DATA_REGIONS",
        names=[('SYSTEM', 0),
               ('INPUT_DATA', 1),
               ('OUTPUT_DATA', 2),
               ('TRANSMISSIONS', 3),
               ('STATE', 4),
               ('NEIGHBOUR_INITIAL_STATES', 5)])

    CORE_APP_IDENTIFIER = 0xBEEF

    def __init__(self, label, columns, rows, string_size, num_string_cols, entries, initiate, function_id, state, constraints=None):
        MachineVertex.__init__(self, label=label, constraints=constraints)

        config = globals_variables.get_simulator().config
        self._buffer_size_before_receive = None
        if config.getboolean("Buffers", "enable_buffered_recording"):
            self._buffer_size_before_receive = config.getint(
                "Buffers", "buffer_size_before_receive")
        self._time_between_requests = config.getint(
            "Buffers", "time_between_requests")
        self._receive_buffer_host = config.get(
            "Buffers", "receive_buffer_host")
        self._receive_buffer_port = helpful_functions.read_config_int(
            config, "Buffers", "receive_buffer_port")

        '''
        all the data that will be transfered to the vertex'''
        self.columns         = columns
        self.rows            = rows
        self.string_size     = string_size
        self.num_string_cols = num_string_cols 
        self.entries         = entries 
        self.initiate        = initiate
        self.function_id     = function_id

        '''
        allocate space for entries and 24 bytes for the 6 integers that make up the header information'''
        self._input_data_size  = (string_size * rows * num_string_cols) + \
                                 (4           * rows * (columns - num_string_cols)) + 24
        self._output_data_size = string_size 

        # app specific elements
        self.placement = None
        self.state = state

    @property
    @overrides(MachineVertex.resources_required)
    def resources_required(self):
        resources = ResourceContainer(
            cpu_cycles=CPUCyclesPerTickResource(45),
            dtcm=DTCMResource(100), sdram=SDRAMResource(100))

        resources.extend(recording_utilities.get_recording_resources(
           [self._output_data_size],
            self._receive_buffer_host, self._receive_buffer_port))
        
        return resources

    @overrides(AbstractHasAssociatedBinary.get_binary_file_name)
    def get_binary_file_name(self):
        return "vertex.aplx"

    @overrides(AbstractHasAssociatedBinary.get_binary_start_type)
    def get_binary_start_type(self):
        return ExecutableStartType.USES_SIMULATION_INTERFACE
    
    def load_data_on_vertices(self,spec,iptags):
        
        # recording data (output) region
        spec.switch_write_focus(self.DATA_REGIONS.OUTPUT_DATA.value)
        spec.write_array(recording_utilities.get_recording_header_array(
            [self._output_data_size], self._time_between_requests,
            self._output_data_size, iptags))   
        
        # input data region
        spec.switch_write_focus(self.DATA_REGIONS.INPUT_DATA.value)
        
        #write header information - 16bytes of information  
        spec.write_array([self.columns, 
                          self.rows,
                          self.string_size,
                          self.num_string_cols,
                          self.initiate,
                          self.function_id])   
        
        #write the string data entries
        for i in range (0, self.num_string_cols):
            for j in range (0, self.rows):
                spec.write_array(
                                 convert_string_to_integer_parcel(self.entries[j][i], #-> entry converted to integers
                                                                  self.string_size))  #-> number of integers used for string

        #write the integer data entries
        for i in range (self.num_string_cols, self.columns):
            for k in range (0, self.rows):
                spec.write_value(int(self.entries[k][i])) #-> those are 32-bit integers by default
                    
    def configure_edges(self,spec,routing_info,machine_graph):
        
        #edge related information
        # check got right number of keys and edges going into me
        partitions = machine_graph.get_outgoing_edge_partitions_starting_at_vertex(self)
        
        if not utility_calls.is_single(partitions):
            raise exceptions.ConfigurationException(
                "Can only handle one type of partition. ") 
            
        # check for duplicates - there is only one edge at the moment for each vertex
        edges = list(machine_graph.get_edges_ending_at_vertex(self))
        if len(set(edges)) != 1:
            output = ""
            for edge in edges:
                output += edge.pre_subvertex.label + " : "
            raise exceptions.ConfigurationException(
                "I've got duplicate edges. This is a error. The edges are "
                "connected to these vertices \n {}".format(output))

        if len(edges) != 1:
            raise exceptions.ConfigurationException(
                "I've not got the right number of connections. I have {} "
                "instead of 1".format(
                    len(machine_graph.incoming_subedges_from_subvertex(self))))

        for edge in edges:
            if edge.pre_vertex == self:
                raise exceptions.ConfigurationException(
                    "I'm connected to myself, this is deemed an error"
                    " please fix.")            
            
        # get the key from this instance of the vertex
        key = routing_info.get_first_key_from_pre_vertex(
            self, self.PARTITION_ID)

        spec.switch_write_focus(
            region=self.DATA_REGIONS.TRANSMISSIONS.value)
        
        #write the key to the designated region
        #0 -> key does not exist, 1 -> key exists
        if key is None:
            spec.write_value(0)  #for some reason key is None
            spec.write_value(0)
        else:
            spec.write_value(1) 
            spec.write_value(key)

        # write state value
        #0 -> vertex dead, 1 -> vertex alive
        spec.switch_write_focus(
            region=self.DATA_REGIONS.STATE.value)

        if self.state: 
            spec.write_value(1)
        else:
            spec.write_value(0)

        # write neighbours data state
        spec.switch_write_focus(
            region=self.DATA_REGIONS.NEIGHBOUR_INITIAL_STATES.value)
        alive = 0
        dead = 0
        
        for edge in edges:
            state = edge.pre_vertex.state
            if state:
                alive += 1
            else:
                dead += 1

        spec.write_value(alive)
        spec.write_value(dead)                                  

    @overrides(MachineDataSpecableVertex.generate_machine_data_specification)
    def generate_machine_data_specification(
            self, spec, placement, machine_graph, routing_info, iptags,
            reverse_iptags, machine_time_step, time_scale_factor):
        
        self.placement = placement

        # Setup words + 1 for flags + 1 for recording size
        setup_size = constants.SYSTEM_BYTES_REQUIREMENT

        # Reserve SDRAM space for memory areas:
        self._reserve_memory_regions(spec, setup_size)

        # set up the SYSTEM partition
        spec.switch_write_focus(self.DATA_REGIONS.SYSTEM.value)
        spec.write_array(simulation_utilities.get_simulation_header_array(
            self.get_binary_file_name(), machine_time_step,
            time_scale_factor))
        
        #load all required data onto the vertex
        Vertex.load_data_on_vertices(self,spec,iptags)
        
        #build all required edges between the vertices
        Vertex.configure_edges(self,spec,routing_info,machine_graph)
        
        # End-of-Spec:
        spec.end_specification()

    def _reserve_memory_regions(self, spec, system_size):
        
        #system data
        spec.reserve_memory_region(
            region=self.DATA_REGIONS.SYSTEM.value, size=system_size,
            label='systemInfo')
        
        #input and output regions
        spec.reserve_memory_region(
            region=self.DATA_REGIONS.INPUT_DATA.value,
            size=self._input_data_size,
            label="Input_data")   
        
        spec.reserve_memory_region(
            region=self.DATA_REGIONS.OUTPUT_DATA.value,
            size=recording_utilities.get_recording_header_size(1),
            label="Output_data")
        
        # edge requirements
        spec.reserve_memory_region(
            region=self.DATA_REGIONS.TRANSMISSIONS.value,
            size=self.TRANSMISSION_DATA_SIZE, label="inputs")
        
        spec.reserve_memory_region(
            region=self.DATA_REGIONS.STATE.value,
            size=self.STATE_DATA_SIZE, label="state")
        
        spec.reserve_memory_region(
            region=self.DATA_REGIONS.NEIGHBOUR_INITIAL_STATES.value,
            size=8, label="neighour_states")    

    def read(self, placement, buffer_manager):
        """ Get the data written into sdram
        :param placement: the location of this vertex
        :param buffer_manager: the buffer manager
        :return: string output
        """
        data_pointer, missing_data = buffer_manager.get_data_for_vertex(
            placement, 0)
        
        if missing_data:
            raise Exception("missing data!")
        record_raw = data_pointer.read_all()
        output = str(record_raw)
        return record_raw

    def get_minimum_buffer_sdram_usage(self):
        return self._input_data_size + self._output_data_size

    def get_n_timesteps_in_buffer_space(self, buffer_space, machine_time_step):
        return recording_utilities.get_n_timesteps_in_buffer_space(
            buffer_space, len("Vertex"))

    def get_recorded_region_ids(self):
        return [2]

    def get_recording_region_base_address(self, txrx, placement):
        return helpful_functions.locate_memory_region_for_placement(
            placement, self.DATA_REGIONS.OUTPUT_DATA.value, txrx)