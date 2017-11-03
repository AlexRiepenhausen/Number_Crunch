//! imports
#include "spin1_api.h"
#include "common-typedefs.h"
#include <data_specification.h>
#include <recording.h>
#include <simulation.h>
#include <debug.h>
#include <circular_buffer.h>

/*! multicast routing keys to communicate with neighbours */
uint my_key;

/*! buffer used to store spikes */
static circular_buffer input_buffer;

//! Variables representing state
uint32_t my_state = 0;
int alive_states_recieved_this_tick = 0;
int dead_states_recieved_this_tick = 0;

//! control value, which says how many timer ticks to run for before exiting
static uint32_t simulation_ticks = 0;
static uint32_t infinite_run = 0;
static uint32_t time = 0;

//! int as a bool to represent if this simulation should run forever
static uint32_t infinite_run;

//! The recording flags
static uint32_t recording_flags = 2;

//! human readable definitions of each region in SDRAM
typedef enum regions_e {
    SYSTEM_REGION,
    INPUT_DATA,
	OUTPUT_DATA,
    TRANSMISSIONS,
    STATE,
    NEIGHBOUR_INITIAL_STATES
} regions_e;

//! values for the priority for each callback
typedef enum callback_priorities{
    MC_PACKET = -1, SDP = 0, USER = 3, TIMER = 2, DMA = 1
} callback_priorities;

//! human readable definitions of each element in the transmission region
typedef enum transmission_region_elements {
    HAS_KEY, MY_KEY
} transmission_region_elements;

//! values for the states
typedef enum states_values{
    ALIVE = 1, DEAD = 0
} states_values;

//! human readable definitions of each element in the initial state
//! region
typedef enum initial_state_region_elements {
    INITIAL_STATE
} initial_state_region_elements;

///////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBAL HEADER INFO - contains information on the structure of data within vertex              //
///////////////////////////////////////////////////////////////////////////////////////////////////

struct header_info {

   unsigned int num_cols;
   /* number of columns
    * in the original csv file
    */
   unsigned int num_rows;
   /* number of rows
    * in the original csv file
    */
   unsigned int string_size;
   /* number of bytes that
    * are allocated for each individual string
    */
   unsigned int num_string_cols;
   /* Tells you the number of string columns
    * All string columns SHOULD be written to SDRAM first (from python) -
    * then and only then the integer columns
    */
   unsigned int initiate_send;
   /* if 1, vertex will be the first one to send out spike
    * if 0, vertex will wait until spike received
    */
   unsigned int function_id;
   /* holds id of function to be invoked
    * 0 - None
    * 1 - Count number of all data entries within the graph
    * 2 - Builds an index table in every core within the network
    */

};

struct header_info header;

///////////////////////////////////////////////////////////////////////////////////////////////////
// DATA ENTRY INDEX - assigns unqiue id's to every data entry within every column           //
///////////////////////////////////////////////////////////////////////////////////////////////////

struct index_info {

	unsigned int *id_index;
	/* Holds the unique identifier for each data entry
	 * Example: id_index[1] contains the unique id for the
	 * second data entry within SDRAM
	 * Currently this works only for one column
	 * Length: header.num_rows
	 */
	unsigned int *message;
	/* Holds 4 integers that make up a string
	 * Designed to take a string entry that has been forwarded
	 * by 4 distinct MCPL packages
	 */
	unsigned int  message_id;
	/* Holds the unique id of string above
	 * Takes the id from an incoming MCPL package as well
	 */
	unsigned int  messages_received;
	/* Keeps track of number of MCPL packages received
	 * if messages_received mod 5 = 0, a string data entry and its
	 * id have been received
	 */
    unsigned int  index_complete;
	/* A flag that tells if the index on this vertex is complete
	 * Complete = 1; Incomplete = 0;
	 * Complete means that there are no indices left with value 0
	 */
	unsigned int  max_id;
	/* Tells you the highest id number on this vertex
	 */

	unsigned int message_0000_sent;
	/* A flag that tells you if this vertex already
	 * sent 0-0-0-0 to its neighbour
	 * If that is the case, all the vertex has to do upon receiving
	 * messages is to forward without invoking update_index()
	 */


};

struct index_info local_index;

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION REFERENCES                                                                           //                                                                  //
///////////////////////////////////////////////////////////////////////////////////////////////////

void resume_callback();
void iobuf_data();
unsigned int compare_two_strings(unsigned int *string_1, unsigned int *string_2);
unsigned int find_instance_of(unsigned int given_id, unsigned int offset);
void send_string_to_next_vertex_with_id(unsigned int data_entry_position);
void send_empty_string_to_next_vertex_with_id(unsigned int id);
void forward_string_message_to_next_vertex_with_id();

void start_processing();
void initialise_index();
void complete_index(unsigned int unique_id, unsigned int start_index);
void update_index();
void index_receive(uint payload);
void index_message_reached_sender();
void count_function_start();
void count_function_receive(uint payload);

void count_function_start();
void count_function_receive(uint payload);

void send_state(uint payload, uint delay);
void receive_data(uint key, uint payload);

void retrieve_header_data();
void record_solution();
void record_string_entry(unsigned int *int_arr);
void record_int_entry(unsigned int solution);

void update(uint ticks, uint b);
static bool initialise_recording();
static bool initialize(uint32_t *timer_period);
void c_main();

///////////////////////////////////////////////////////////////////////////////////////////////////
// GENERIC UTILITY FUNCTIONS                                                                     //
// RESUME_CALLBACK, IOBUF_DATA, COMPARE_TWO_STRINGS, FIND_INSTANCE_OF,                           //
// SEND_STRING_TO_NEXT_VERTEX_WITH_ID                                                            //
///////////////////////////////////////////////////////////////////////////////////////////////////

void resume_callback() {
    time = UINT32_MAX;
}

void iobuf_data(){
    address_t address = data_specification_get_data_address();
    address_t data_address =
        data_specification_get_region(INPUT_DATA, address);

    log_info("Data address is %08x", data_address);

    int* my_string = (int *) &data_address[0];
    log_info("Data read is: %s", my_string);
}

unsigned int compare_two_strings(unsigned int *string_1, unsigned int *string_2) {

    //compares two strings with each other
	//return 1
	//return 0 if not

    unsigned int i;
    for(i = 0; i < header.string_size/4; i++){
      if(string_1[i] != string_2[i]){return 0;}
    }

	return 1;

}

unsigned int find_instance_of(unsigned int given_id, unsigned int offset) {

	//finds the data item with the given id and returns its index(!),
	//starting from a certain offset, so that iterating through all items with the
	//same id is possible if required

	//offset is the index of the last element that has been found in the list
	unsigned int i = offset;
    for(i = 0; i < header.num_rows; i++) {
    	if(local_index.id_index[i] == given_id){
    		return i;
    	}
    }

    return -1;

}

void send_string_to_next_vertex_with_id(unsigned int data_entry_position) {

	//take every column of strings and assign an unique id to each string
    address_t address = data_specification_get_data_address();
    address_t data_address =
        data_specification_get_region(INPUT_DATA, address);

	//make sure any non-integer entries do exist
	if(header.num_string_cols != 0 && header.num_rows != 0) {

    	unsigned int start = 6  + 4 * data_entry_position;
    	unsigned int end   = 10 + 4 * data_entry_position;
    	unsigned int count = 0;

    	unsigned int i;
    	unsigned int entry[4];

    	//current entry
	    for(i = start; i < end; i++) {
	    	entry[count] = *(&data_address[i]);
	    	count++;
	    }

		//send the first data entry to the next core - 4 spikes
		send_state(entry[0], 1);
		send_state(entry[1], 1);
		send_state(entry[2], 1);
		send_state(entry[3], 1);

		//send the id of that entry
		send_state(local_index.id_index[data_entry_position], 1);

	}

}

void send_empty_string_to_next_vertex_with_id(unsigned int id) {

	//take every column of strings and assign an unique id to each string
	send_state( 0, 1);
	send_state( 0, 1);
	send_state( 0, 1);
	send_state( 0, 1);
	send_state(id, 1);

}

void forward_string_message_to_next_vertex_with_id() {

	send_state(local_index.message[0], 1);
	send_state(local_index.message[1], 1);
	send_state(local_index.message[2], 1);
	send_state(local_index.message[3], 1);
	send_state(local_index.message_id, 1);

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN COMPONENTS OF QUERY PROCESSING ALGORITHMS                                                //
// START_PROCESSING                                                                              //
///////////////////////////////////////////////////////////////////////////////////////////////////

void start_processing() {

	switch(header.function_id) {

		case 1 :

	         count_function_start();

	         break;

		case 2 :

			 initialise_index();

			 //start if index is complete - this only happens with the leader vertex in the beginning
			 if(local_index.index_complete == 1) {
				 send_string_to_next_vertex_with_id(0); //-> first entry
			 }

			 break;

	    default :
	    	log_info("No function selected");

	}

}

void initialise_index() {

	//take every column of strings and assign an unique id to each string
    local_index.id_index          = malloc(sizeof(unsigned int) * header.num_rows);
    local_index.message           = malloc(sizeof(unsigned int) * 4);
    local_index.message_id        = 0;
    local_index.messages_received = 0;
    local_index.index_complete    = 0;
    local_index.max_id            = 0;
    local_index.message_0000_sent = 0;

	//make sure any non-integer entries do exist
	if(header.num_string_cols != 0 && header.num_rows != 0) {

		//if you are the vertex that starts all of this:
		if(header.initiate_send == 1) {
			//id = 1, start position = 0
			complete_index(1,0);
		}
		else {

			//if you are not the vertex who starts this, assign 0 to every index
			unsigned int i = 0;
		    for(i = 0; i < header.num_rows; i++) {
		    	local_index.id_index[i] = 0;
		    }

		}

	}

}

void complete_index(unsigned int unique_id, unsigned int start_index) {

	//get the SDRAM address
    address_t address = data_specification_get_data_address();
    address_t data_address =
        data_specification_get_region(INPUT_DATA, address);

	//read the first single entry
	unsigned int i;
    unsigned int j;

    unsigned int current_entry[4];

    for(i = start_index; i < header.num_rows; i++) {

    	unsigned int start = 6  + 4*i;
    	unsigned int end   = 10 + 4*i;
    	unsigned int count = 0;

    	//current entry
	    for(j = start; j < end; j++) {
	    	current_entry[count] = *(&data_address[j]);
	    	count++;
	    }

	    //check if entry exists in the already assigned region
	    //Since the number of entries on one core is always limited, this is fine

        unsigned int k = 0;
        unsigned int l = 0;

        unsigned int assigned = 0;

        unsigned int past_entry[4];

        //check if id has been already assigned
        if(local_index.id_index[i] == 0) {

        	//if id is 0, check for identical previous entries
            for(k = 0; k < i; k++) {

    	    	unsigned int start2 = 6  + 4*k;
    	    	unsigned int end2   = 10 + 4*k;
    	    	unsigned int count2 = 0;

    	    	//past entry
    		    for(l = start2; l < end2; l++) {
    		    	past_entry[count2] = *(&data_address[l]);
    		    	count2++;
    		    }

    		    if(compare_two_strings(past_entry,current_entry) == 1) {
    		    	local_index.id_index[i] = local_index.id_index[k];
    		    	assigned = 1;
                    break;
    		    }

            }

        }
        else {
        	//else make sure you do not assign unqiue_id if the id_index is not 0
        	assigned = 1;
        }

        //if no index assigned yet - that is the entry has not been spotted:
        if(assigned == 0){
        	local_index.id_index[i] = unique_id;
        	unique_id++;
        }

    }

    //all data entries have a non zero index assigned to them
    local_index.max_id = unique_id - 1;
    local_index.index_complete = 1;

}

void update_index() {

	//check if a 0-0-0-0 message has been sent by this vertex in the past -
	//if that is the case, no updating is needed
    if(local_index.message_0000_sent != 1) {

    	//goes through the index and replaces all elements that match with the
    	//spike message with the id contained within the message
        address_t address = data_specification_get_data_address();
        address_t data_address =
            data_specification_get_region(INPUT_DATA, address);

    	unsigned int i,j;
        for(i = 0; i < header.num_rows; i++) {

        	unsigned int start = 6  + 4 * i;
        	unsigned int end   = 10 + 4 * i;
        	unsigned int count = 0;

        	unsigned int current_entry[4];

        	//current entry
    	    for(j = start; j < end; j++) {
    	    	current_entry[count] = *(&data_address[j]);
    	    	count++;
    	    }

        	if(compare_two_strings(current_entry,local_index.message) == 1) {

        		//check if the id has already been assigned;
        		//if that is the case you completed the ring (as in the message went through all cores already)
        		if(local_index.id_index[i] != 0) {
        			index_message_reached_sender();
        		}
        		else {
        			//update the index if it is still 0
            	    local_index.id_index[i] = local_index.message_id;
        		}

        	}

        }//for

    }//if message_0000_sent != 1


}

void index_receive(uint payload) {

	//wait until all 5 spikes received
	//4 Spikes for the string info - 1 for the id

	local_index.messages_received++;

	if(local_index.messages_received % 5 == 0) {

		local_index.message_id = payload;

		//Check if all messages are 0
		unsigned int i;
		unsigned int flag = 1;
		for(i = 0; i < 4; i++){
			if(local_index.message[i] != 0){
				flag = 0;
				break;
			}
		}

		//not a 0-0-0-0 message
		if(flag == 0){
			update_index();
		    //forward string to next vertex
			forward_string_message_to_next_vertex_with_id();
		}

		//a 0-0-0-0 message -> That means
		//that the previous vertex already has a complete index AND
		//the vertex made sure that all its data entry indices
		//are up to date across the network -> For this processing step,
		//the current vertex now becomes the leader
		if(flag == 1) {

			//terminate the proces if message_0000_sent == 1 -
			//that means that all id_indexes in all vertices had been synchronised
			if(local_index.message_0000_sent == 1){
              //finish
			}
			else {

				//Make sure that the index is complete - if not, take necessary steps
				int zeros_exist = find_instance_of(0,0); //find first occurence of 0 index

				//zeros exist
				if(zeros_exist != -1) {
					//id = received message id, zeros_exist points to first item with id 0
					complete_index(local_index.message_id, zeros_exist);

					unsigned int data_entry_position = find_instance_of(local_index.message_id, 0);
					send_string_to_next_vertex_with_id(data_entry_position);

				}

				//zeros don't exist -the index is complete:
				//Here the index was completed through update_index() alone - that means that
				//there are no new items which can take an id that does not occurr in the previous vertex
				if(zeros_exist == -1) {
					local_index.index_complete = 1;
					forward_string_message_to_next_vertex_with_id();
					local_index.message_0000_sent = 1;
				}

			}

		}


	}
	else {
		//take incoming message
		local_index.message[(local_index.messages_received % 5) - 1] = payload;
	}

}

void index_message_reached_sender() {

	//1st scenario: The message_id is smaller than the max_id
	if(local_index.message_id < local_index.max_id) {

		//take the entry with id local_index.message_id + 1 and repeat the process
		int index = find_instance_of(local_index.message_id + 1, 0);

		//send a new entry around with its id
		send_string_to_next_vertex_with_id(index);

	}

	//2nd scenario: The message_id is exactly the same as the max_id
	if(local_index.message_id == local_index.max_id) {

		//forward 0 0 0 0 and max_id + 1
		send_empty_string_to_next_vertex_with_id(local_index.max_id + 1);

		//set local_index.message_0000_sent to 1,
		//so that update_index doesn't need to be invoked again
		local_index.message_0000_sent = 1;

	}

}

void count_function_start() {

	//counts all data entries within the graph
	//the current implementation relies upon a ring structure

	//send the first MCPL package if initiate is TRUE
	if(header.initiate_send == 1) {
		log_info("solution: %d", header.num_rows);
		record_int_entry(header.num_rows);
		send_state(header.num_rows, 1);
	}

}

void count_function_receive(uint payload) {
    //forward the message to the next vertex (ring)

	//if we have reached the original vertex, stop the entire mechanism
	if(header.initiate_send == 0){
		payload = payload + header.num_rows;
		send_state(payload, 1);
		log_info("solution: %d", payload);
		record_int_entry(payload);
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// DATA TRANSFER BETWEEN CORES                                                                   //
// SEND_STATE, RECEIVE_DATA                                                                      //
///////////////////////////////////////////////////////////////////////////////////////////////////

void send_state(uint payload, uint delay) {

    // reset for next iteration
    alive_states_recieved_this_tick = 0;
    dead_states_recieved_this_tick = 0;

    // send my new state to the simulation neighbours
    while (!spin1_send_mc_packet(my_key, payload, WITH_PAYLOAD)) {
        spin1_delay_us(delay);
    }

    log_debug("sent my state via multicast");

}

void receive_data(uint key, uint payload) {

   //uint key: packet routing key - provided by the RTS
   //uint payload: packet payload - provided by the RTS

   log_info("the key i've received is %d\n", key);
   log_info("the payload i've received is %d\n", payload);

   //If there was space to add spike to incoming spike queue
   if (!circular_buffer_add(input_buffer, payload)) {
       log_info("Could not add state");
   }

   //depending on the function, select a way to handle the incoming message
   switch(header.function_id) {
		case 1 :
			 count_function_receive(payload);
	         break;
		case 2 :
			 index_receive(payload);
	         break;
	    default :
	    	log_info("No function selected");
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// VERTEX INTERNAL DATA MANAGEMENT                                                               //
// RETRIEVE_DATA, RECORD_DATA, RECORD_STRING_ENTRY, RECORD_INT_ENTRY                             //
///////////////////////////////////////////////////////////////////////////////////////////////////

void retrieve_header_data() {

    uint chip = spin1_get_chip_id();
    uint core = spin1_get_core_id();
    log_info("Issuing 'Vertex' from chip %d, core %d", chip, core);

    //access the partition of the SDRAM where input data is stored
    address_t address = data_specification_get_data_address();
    address_t data_address =
        data_specification_get_region(INPUT_DATA, address);

    //header data that contains:
    //- The data description
    //- Processing instructions
    header.num_cols        = data_address[0];
	header.num_rows        = data_address[1];
	header.string_size     = data_address[2];
	header.num_string_cols = data_address[3];
	header.initiate_send   = data_address[4];
	header.function_id     = data_address[5];

	//log this information to iobuf
	log_info("Num_cols: %d", header.num_cols );
	log_info("Num_rows: %d", header.num_rows);
	log_info("string_size: %d", header.string_size);
	log_info("flag : %d", header.num_string_cols);
	log_info("initate_send : %d", header.initiate_send);
	log_info("function_id: %d", header.initiate_send);

}

void record_solution() {

    //access the partition of the SDRAM where input data is stored
    address_t address = data_specification_get_data_address();
    address_t data_address =
        data_specification_get_region(INPUT_DATA, address);

    unsigned int i = 0;
    unsigned int entry1[4];
    for(i = 6; i < 10; i++){
    	entry1[i-6] = *(&data_address[i]);
    }

    //record the data entry in the first recording region (which is OUTPUT)
    record_string_entry(entry1);

}

void record_string_entry(unsigned int *int_arr) {

	//convert the array of [size] integers to a 4*[size] char array
	unsigned char buffer[header.string_size];

	unsigned int i;
	unsigned int size = header.string_size/4;

	for(i = 0; i < size; i++) {
      buffer[size*i + 0] = (int_arr[i] >> 24) & 0xFF;
	  buffer[size*i + 1] = (int_arr[i] >> 16) & 0xFF;
	  buffer[size*i + 2] = (int_arr[i] >> 8) & 0xFF;
	  buffer[size*i + 3] =  int_arr[i] & 0xFF;
	}

    log_info("Entry : %d", buffer);

    //record the data entry in the first recording region (which is OUTPUT)
    bool recorded = recording_record(0, buffer, header.string_size * sizeof(unsigned char));

    if (!recorded) {
        log_info("Information was not recorded...");
    }

}

void record_int_entry(unsigned int solution) {

	char result[16];
	itoa(solution,result,10);

    bool recorded = recording_record(0, result, header.string_size * sizeof(unsigned char));

    if (!recorded) {
        log_info("Information was not recorded...");
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS THAT CONSTITUTE THE MAIN BUILDING BLOCKS OF THE VERTEX:                             //
// UPDATE, INITIALIZE, INITIALIZE_RECORDING, C_MAIN                                              //
///////////////////////////////////////////////////////////////////////////////////////////////////

void update(uint ticks, uint b) {
    use(b);
    use(ticks);

    time++;

    log_info("on tick %d of %d", time, simulation_ticks);

    // check that the run time hasn't already elapsed and thus needs to be killed
    if ((infinite_run != TRUE) && (time >= simulation_ticks)) {
        log_info("Simulation complete.\n");

        if (recording_flags > 0) {
            log_info("updating recording regions");
            recording_finalise();
        }

        // falls into the pause resume mode of operating
        simulation_handle_pause_resume(resume_callback);

        return;

    }

    if (time == 1) {
    	retrieve_header_data();
    	start_processing();
    } else if (time == 100) {
        iobuf_data();
    }

    // trigger buffering_out_mechanism
    log_info("recording flags is %d", recording_flags);
    if (recording_flags > 0) {
        log_info("doing timer tick update\n");
        recording_do_timestep_update(time);
        log_info("done timer tick update\n");
    }
}

static bool initialise_recording(){

	//! \brief Initialises the recording parts of the model
	//! \return True if recording initialisation is successful, false otherwise

    address_t address = data_specification_get_data_address();
    address_t recording_region = data_specification_get_region(
        OUTPUT_DATA, address);
    bool success = recording_initialize(recording_region, &recording_flags);
    log_info("Recording flags = 0x%08x", recording_flags);
    return success;

}

static bool initialize(uint32_t *timer_period) {

    // Get the address this core's DTCM data starts at from SRAM
    address_t address = data_specification_get_data_address();

    // Read the header
    if (!data_specification_read_header(address)) {
        log_info("failed to read the data spec header");
        return false;
    }

    // Get the timing details and set up the simulation interface
    if (!simulation_initialise(
            data_specification_get_region(SYSTEM_REGION, address),
            APPLICATION_NAME_HASH, timer_period, &simulation_ticks,
            &infinite_run, SDP, DMA)) {
        return false;
    }

    // initialise transmission keys
    address_t transmission_region_address = data_specification_get_region(
            TRANSMISSIONS, address);
    if (transmission_region_address[HAS_KEY] == 1) { //for some reason this is 0
        my_key = transmission_region_address[MY_KEY];
        log_info("my key is %d\n", my_key);
    } else {
        log_error(
            "this conways cell can't effect anything, deduced as an error,"
            "please fix the application fabric and try again\n");
        return false;
    }

    // read my state
    address_t my_state_region_address = data_specification_get_region(
        STATE, address);
    my_state = my_state_region_address[INITIAL_STATE];
    log_info("my initial state is %d\n", my_state);

    // read neighbour states for initial tick
    address_t my_neigbhour_state_region_address = data_specification_get_region(
        NEIGHBOUR_INITIAL_STATES, address);
    alive_states_recieved_this_tick = my_neigbhour_state_region_address[0];
    dead_states_recieved_this_tick = my_neigbhour_state_region_address[1];

    // initialise my input_buffer for receiving packets
    input_buffer = circular_buffer_initialize(256);
    if (input_buffer == 0){
        return false;
    }
    log_info("input_buffer initialised");

    return true;
}

void c_main() {

    log_info("starting vertex test\n");

    // Load DTCM data
    uint32_t timer_period;

    // initialise the model
    if (!initialize(&timer_period)) {
        rt_error(RTE_SWERR);
    }

    // initialise the recording section
    // set up recording data structures
    if(!initialise_recording()){
         rt_error(RTE_SWERR);
    }

    // set timer tick value to configured value
    log_info("setting timer to execute every %d microseconds", timer_period);
    spin1_set_timer_tick(timer_period);

    // register callbacks
    spin1_callback_on(MCPL_PACKET_RECEIVED, receive_data, MC_PACKET);
    spin1_callback_on(TIMER_TICK, update, TIMER);

    // start execution
    log_info("Starting\n");

    // Start the time at "-1" so that the first tick will be 0
    time = UINT32_MAX;

    simulation_run();
}
