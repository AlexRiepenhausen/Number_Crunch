//! imports
#include "spin1_api.h"
#include "common-typedefs.h"
#include <data_specification.h>
#include <recording.h>
#include <simulation.h>
#include <debug.h>

/* Debugging mode */
#define DEBUG 0
/* 0 Default information about cores
 * 1 Enables information about messages received and sent
 * 2 Debug info on the id distribution algorithm
 */

/*! multicast routing keys to communicate with neighbours */
unsigned int *key_values;

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
    MC_PACKET = 3, SDP = -1, USER = 2, TIMER = 1, DMA = 0
} callback_priorities;

//! human readable definitions of each element in the transmission region
typedef enum transmission_region_elements {
    KEY_ONE_EXISTS, KEY_ONE, KEY_TWO_EXISTS, KEY_TWO
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

   unsigned int processor_id;
   /* The id of the processor */

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

};

//declare linked list to be a dictionary
typedef struct node {
	unsigned int *string;
	unsigned int id;
	unsigned int frequency;
    struct node *next;
} node_t;

node_t * dictionary;
node_t * dict_start;

struct index_info local_index;

unsigned int reported_ready;
unsigned int forward_mode_on;
unsigned int current_leader;
unsigned int global_max_id;
unsigned int in_charge;

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION REFERENCES                                                                           //                                                                  //
///////////////////////////////////////////////////////////////////////////////////////////////////

void resume_callback();
void iobuf_data();

int search_dictionary(unsigned int *given_string);
void add_item_to_dictionary(unsigned int *given_string, unsigned int id);

unsigned int compare_two_strings(unsigned int *string_1, unsigned int *string_2);
unsigned int find_instance_of(unsigned int given_id, unsigned int offset);
unsigned int identify_signal(unsigned int signal);

void forward_string();
void send_string(unsigned int data_entry_position);
void send_signal(unsigned int id, unsigned int signal);

void start_processing();
void initialise_index();
void complete_index(unsigned int unique_id, unsigned int start_index);
void update_index_upon_message_received();
void index_receive(uint payload);
void index_message_reached_sender();

void count_function_start();
void count_function_receive(uint payload);

void leader_blast();
void leader_collects_reports(uint payload);
void report_to_leader(uint payload);

void count_function_start();
void count_function_receive(uint payload);

void send_state(uint payload, uint key);
void receive_data(uint key, uint payload);

void retrieve_header_data();
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

    int* my_string = (int *) &data_address[0];
}

int search_dictionary(unsigned int *given_string) {

	log_info("ITEM TO SEARCH:");
	log_info("-item1: %d",given_string[0]);
	log_info("-item1: %d",given_string[1]);
	log_info("-item1: %d",given_string[2]);
	log_info("-item1: %d",given_string[3]);
	log_info("-----------------");
	node_t * item = dictionary;
    //iterate through dictionary
	while(item->id != -1) {

		log_info("-item2: %d",item->string[0]);
		log_info("-item2: %d",item->string[1]);
		log_info("-item2: %d",item->string[2]);
		log_info("-item2: %d",item->string[3]);

		if(compare_two_strings(given_string,item->string) == 1){
			log_info("-----------------");
			return item->id;
		}

		item = item->next;

	}

	//item not found
	log_info("-----------------");
	return -1;

}

void add_item_to_dictionary(unsigned int *given_string, unsigned int id) {

	node_t * item = dictionary;

	log_info("item->id: %d", item->id);

    //iterate through dictionary
	while(item->id != -1) {
		log_info("steps");
		item = item->next;
	}

	item->string = given_string;
	item->id     = id;

	node_t *new = malloc(sizeof(node_t));
	new->id = -1;
	new->string = malloc(4 * sizeof(unsigned int));

	item->next = new;

	log_info("DICTIONARYID: %d", dictionary->next->id);
	log_info("DICTIONARYID: %d", item->next->id);



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

void send_string(unsigned int data_entry_position) {

	//take every column of strings and assign an unique id to each string
    address_t address = data_specification_get_data_address();
    address_t data_address =
        data_specification_get_region(INPUT_DATA, address);

	//make sure any non-integer entries do exist
	if(header.num_string_cols != 0 && header.num_rows != 0) {

    	unsigned int start = 7  + 4 * data_entry_position;
    	unsigned int end   = 11 + 4 * data_entry_position;
    	unsigned int count = 0;

    	unsigned int i;
    	unsigned int entry[4];

    	//current entry
	    for(i = start; i < end; i++) {
	    	entry[count] = *(&data_address[i]);
	    	count++;
	    }

		//send the first data entry to the next core - 4 spikes
		send_state(entry[0], 2);
		send_state(entry[1], 2);
		send_state(entry[2], 2);
		send_state(entry[3], 2);

		//send the id of that entry
		send_state(local_index.id_index[data_entry_position], 2);

		#if defined(DEBUG) && (DEBUG >= 1)
			log_info("SEND STRING");
			log_info("M1: %d",entry[0]);
			log_info("M2: %d",entry[1]);
			log_info("M3: %d",entry[2]);
			log_info("M4: %d",entry[3]);
			log_info("M5: %d",local_index.id_index[data_entry_position]);
		#endif

	}

}

void forward_string() {

	send_state(local_index.message[0], 2);
	send_state(local_index.message[1], 2);
	send_state(local_index.message[2], 2);
	send_state(local_index.message[3], 2);
	send_state(local_index.message_id, 2);

    #if defined(DEBUG) && (DEBUG >= 1)
		log_info("FORWARD");
		log_info("M1: %d",local_index.message[0]);
		log_info("M2: %d",local_index.message[1]);
		log_info("M3: %d",local_index.message[2]);
		log_info("M4: %d",local_index.message[3]);
		log_info("M5: %d",local_index.message_id);
	#endif

}

unsigned int identify_signal(unsigned int signal) {

	unsigned int i;
	for(i = 0; i < 3; i++) {
		if(local_index.message[i] != signal) {
			return 0;
		}
	}

	return 1;

}

void send_signal(unsigned int id, unsigned int signal) {

	//0 processing finished completely
	//1 core ready for other tasks

	//take every column of strings and assign an unique id to each string
	send_state(signal, 2);
	send_state(signal, 2);
	send_state(signal, 2);
	send_state(header.processor_id, 2);
	send_state(id, 2);

	#if defined(DEBUG) && (DEBUG >= 1)
		log_info("SIGNAL");
		log_info("M1: %d",signal);
		log_info("M2: %d",signal);
		log_info("M3: %d",signal);
		log_info("M4: %d",header.processor_id);
		log_info("M5: %d",id);
	#endif

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
				 //record the index information
				 for(unsigned int i = 0; i < header.num_rows; i++) {
					 record_int_entry(local_index.id_index[i]);
				 }
				 global_max_id = 1;
				 send_string(0); //-> first entry
			 }

			 break;

		case 3 :

			 //if you are the leader
			 if(header.initiate_send == 1) {leader_blast();}
		     break;

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

    dictionary         = malloc(sizeof(node_t));
    dictionary->string = malloc(4 * sizeof(unsigned int));
    dictionary->id     = -1;

    unsigned int current_index = 1;

	//make sure any non-integer entries do exist
	if(header.num_string_cols != 0 && header.num_rows != 0) {

		if(header.initiate_send == 1) {

			//get the SDRAM address
		    address_t address = data_specification_get_data_address();
		    address_t data_address =
		        data_specification_get_region(INPUT_DATA, address);

			//read the first single entry
			unsigned int i;
		    unsigned int j;
		    unsigned int *current_entry = malloc(4 * sizeof(unsigned int));
		    for(i = 0; i < header.num_rows; i++) {

		    	unsigned int start = 7  + 4*i;
		    	unsigned int end   = 11 + 4*i;
		    	unsigned int count = 0;

		    	//current entry
			    for(j = start; j < end; j++) {
			    	current_entry[count] = *(&data_address[j]);
			    	count++;
			    }

			    int result = search_dictionary(current_entry);

			    log_info("RESULT: %d",result);

			    //entry exists in dictionary
			    if(result != -1) {
			    	local_index.id_index[i] = result;
			    }

			    //entry does not exist in dictionary
			    if(result == -1) {
			    	local_index.id_index[i] = current_index;
			    	add_item_to_dictionary(current_entry, current_index);
			    	local_index.max_id = current_index;
			    	current_index++;
			    }

		    }

		    //all data entries have a non zero index assigned to them
		    local_index.index_complete = 1;

		}

		log_info("linkedlist:");
		int *test = malloc(4 * sizeof(unsigned int));
		test[0] = 0;
		test[1] = 0;
		test[2] = 0;
		test[3] = 0;
		int result = search_dictionary(test);

		if(header.initiate_send == 0) {
			for(unsigned int i = 0; i < header.num_rows; i++) {
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

    	unsigned int start = 7  + 4*i;
    	unsigned int end   = 11 + 4*i;
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

        //check if id has already been assigned
        if(local_index.id_index[i] == 0) {

        	//if id is 0, check for identical previous entries
            for(k = 0; k < i; k++) {

    	    	unsigned int start2 = 7  + 4*k;
    	    	unsigned int end2   = 11 + 4*k;
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
        	assigned = 1;
        }

        //if no index assigned yet - that is the entry has not been spotted:
        if(assigned == 0){
        	local_index.id_index[i] = unique_id;
    	    //make sure that local_index.max_id is updated as well
			if(local_index.max_id < unique_id){
    			local_index.max_id = unique_id;
			}

        	unique_id++;
        }

    }

    //all data entries have a non zero index assigned to them
    local_index.index_complete = 1;

}

void update_index_upon_message_received() {

	//goes through the index and replaces all elements that match with the
	//spike message with the id contained within the message
    address_t address = data_specification_get_data_address();
    address_t data_address =
        data_specification_get_region(INPUT_DATA, address);

	unsigned int i,j;
    for(i = 0; i < header.num_rows; i++) {

    	unsigned int start = 7  + 4 * i;
    	unsigned int end   = 11 + 4 * i;
    	unsigned int count = 0;

    	unsigned int current_entry[4];

    	//current entry
	    for(j = start; j < end; j++) {
	    	current_entry[count] = *(&data_address[j]);
	    	count++;
	    }

    	if(compare_two_strings(current_entry,local_index.message) == 1) {

    		if(local_index.id_index[i] == 0) {

    			//update the index if it is still 0
        	    local_index.id_index[i] = local_index.message_id;

        	    //make sure that local_index.max_id is updated as well
    			if(local_index.max_id < local_index.message_id){
        			local_index.max_id = local_index.message_id;
    			}

    		}
    		else {
    			//nothing to update
    			break;
    		}

    	}

    }//for

}

void index_receive(uint payload) {

	//Case 1: You are the leader and waiting for reports
	if(header.processor_id % 16 == 0 && forward_mode_on == 0) {

		unsigned int cores_to_report = 15;
		if(current_leader != header.processor_id) {
			cores_to_report = 14;
		}

		//see if everyone is ready
		if(payload == -1){reported_ready++;}
		if(reported_ready == cores_to_report){

			reported_ready = 0;

			if(current_leader % 16 != 0) {
				global_max_id++;
				send_signal(global_max_id,1);
				forward_mode_on = 1;
			}

			if(current_leader % 16 == 0) {

				global_max_id++;
				if(global_max_id <= local_index.max_id) {
					send_string(find_instance_of(global_max_id,0));
				}
				else {
	                send_signal(global_max_id,0);
					current_leader = 1; //-> next core becomes the leader
					forward_mode_on = 1;
				}

			}

		}

	}

	//Case 2: You are the original leader - now forwarding messages
	if(header.processor_id % 16 == 0 && forward_mode_on == 1) {

		local_index.messages_received++;

		//collect up to 5 messages
		if(local_index.messages_received % 5 != 0) {

			local_index.message[(local_index.messages_received % 5) - 1] = payload;

            #if defined(DEBUG) && (DEBUG >= 2)
				log_info("RECEIVED: %d", payload);
			#endif

			//temporary fix for a glitch - band aid
			if(payload == -1) {
				local_index.messages_received--;
			}

		}
		else{

			local_index.message_id = payload;

			#if defined(DEBUG) && (DEBUG >= 2)
				log_info("RECEIVED: %d", payload);
				log_info("------------------");
			#endif

			if(identify_signal(0) == 0) {
				forward_string();
				forward_mode_on = 0;
			}

			if(identify_signal(0) == 1) {

				current_leader++;
				if(current_leader <= 15) {
					forward_string(); //-> next core becomes the leader

				#if defined(DEBUG) && (DEBUG >= 2)
					log_info("max_id: %d", local_index.message_id);
					log_info("leader: %d", local_index.message[3]);
				#endif

					forward_mode_on = 1;
				}

			}

		}

	}

	//Case 3: You are any one of the subordinates
	if(header.processor_id % 16 != 0) {

		local_index.messages_received++;

		//collect up to 5 messages
		if(local_index.messages_received % 5 != 0) {
			local_index.message[(local_index.messages_received % 5) - 1] = payload;
            #if defined(DEBUG) && (DEBUG >= 2)
				log_info("RECEIVED: %d", payload);
            #endif
		}
		else {

			local_index.message_id = payload;

			#if defined(DEBUG) && (DEBUG >= 2)
				log_info("RECEIVED: %d", payload);
				log_info("------------------");
			#endif

			if(in_charge == 0) {

				//ignore 1-1-1 messages
				if(identify_signal(1) == 1) {return;}

				if(identify_signal(0) == 1) {
					if(local_index.message[3]+1 == header.processor_id){

						in_charge = 1; //-> you are the new leader

			     		//Make sure that the index is complete
						int zeros_exist = find_instance_of(0,0); //find first occurence of 0 index

						//zeros exist
						if(zeros_exist != -1) {
							complete_index(local_index.message_id, zeros_exist);
							send_string(zeros_exist);
						}

						//zeros don't exist
						if(zeros_exist == -1) {
							in_charge = 0;
							local_index.index_complete = 1;
							send_signal(local_index.message_id,0);
						}

		    		    for(unsigned int i = 0; i < header.num_rows; i++) {
		    		        record_int_entry(local_index.id_index[i]);
		    		    }

					}
				}

				if(identify_signal(0) == 0) {

					if(local_index.index_complete == 0) {
						update_index_upon_message_received();
					}

					send_state(-1, 2); //report ready

				}

			}//if not in charge

			if(in_charge == 1) {

				if(identify_signal(1) == 1) {

					#if defined(DEBUG) && (DEBUG >= 2)
						log_info("SPARTA: %d", local_index.message_id);
						log_info("received: %d", local_index.message_id);
						log_info("max_id:   %d", local_index.max_id);
					#endif

					if(local_index.message_id <= local_index.max_id) {
						send_string(find_instance_of(local_index.message_id,0));
					}
					else {
		                send_signal(local_index.message_id,0);
		                log_info("OLD LEADER: %d",header.processor_id);
						log_info("max_id: %d", local_index.message_id);
		                in_charge = 0;
					}
				}

			}//if in charge


		}

	}

}

void count_function_start() {

	//counts all data entries within the graph
	//the current implementation relies upon a ring structure

	//send the first MCPL package if initiate is TRUE
	if(header.initiate_send == 1) {
		record_int_entry(header.num_rows);
		send_state(header.num_rows, 1);
	}

}

void count_function_receive(uint payload) {

	//if we have reached the original vertex, stop the entire mechanism
	if(header.initiate_send == 0){
		payload = payload + header.num_rows;
		send_state(payload, 1);
		record_int_entry(payload);
	}

}

void leader_blast() {
	send_state(1, 2);
}

void report_to_leader(uint payload) {
	record_int_entry(payload);
	send_state(payload,2);
}

unsigned int report = 0;
unsigned int reports_received = 0;
void leader_collects_reports(uint payload) {

	reports_received++;
	if(reports_received < 15) {
	    report = report + payload;
	}
	else {
		record_int_entry(report + payload);
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// DATA TRANSFER BETWEEN CORES                                                                   //
// SEND_STATE, RECEIVE_DATA                                                                      //
///////////////////////////////////////////////////////////////////////////////////////////////////

void send_state(uint payload, uint partition_number) {

    // reset for next iteration
    alive_states_recieved_this_tick = 0;
    dead_states_recieved_this_tick = 0;

    // send my new state to the simulation neighbours
    while (!spin1_send_mc_packet(key_values[partition_number-1], payload, WITH_PAYLOAD)) {
        spin1_delay_us(1);
    }

}

void receive_data(uint key, uint payload) {

   //uint key: packet routing key - provided by the RTS
   //uint payload: packet payload - provided by the RTS

   //log_info("the key i've received is %d\n", key);
   //log_info("the payload i've received is %d\n", payload);

	#if defined(DEBUG) && (DEBUG >= 1)
    	log_info("--package arrived-- %d", payload);
	#endif

   //depending on the function, select a way to handle the incoming message
   switch(header.function_id) {
		case 1 :
			 count_function_receive(payload);
	         break;
		case 2 :
			 index_receive(payload);
	         break;
		case 3 :
			 if(header.initiate_send == 0){report_to_leader(payload);}
			 else{leader_collects_reports(payload);}
			 break;
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
    header.processor_id    = data_address[0];
    header.num_cols        = data_address[1];
	header.num_rows        = data_address[2];
	header.string_size     = data_address[3];
	header.num_string_cols = data_address[4];
	header.initiate_send   = data_address[5];
	header.function_id     = data_address[6];

	reported_ready  = 0;
	forward_mode_on = 0;
	global_max_id   = 0;
	in_charge       = 0;
	current_leader  = 0;

	if(header.initiate_send == 1) {
		current_leader = header.processor_id;
	}

	//log this information to iobuf
	log_info("Processor id: %d", header.processor_id );
	log_info("Num_cols: %d", header.num_cols );
	log_info("Num_rows: %d", header.num_rows);
	log_info("string_size: %d", header.string_size);
	log_info("flag : %d", header.num_string_cols);
	log_info("initate_send : %d", header.initiate_send);
	log_info("function_id: %d", header.function_id);

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

    //log_info("String Entry : %s", buffer);

    //record the data entry in the first recording region (which is OUTPUT)
    bool recorded = recording_record(0, buffer, header.string_size * sizeof(unsigned char));

}

void record_int_entry(unsigned int solution) {

	char result[10];
	itoa(solution,result,10);

    //unsigned integers take 10 chars if represented as char arrays
    bool recorded = recording_record(0, result, 10 * sizeof(unsigned char));

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS THAT CONSTITUTE THE MAIN BUILDING BLOCKS OF THE VERTEX:                             //
// UPDATE, INITIALIZE, INITIALIZE_RECORDING, C_MAIN                                              //
///////////////////////////////////////////////////////////////////////////////////////////////////

void update(uint ticks, uint b) {
    use(b);
    use(ticks);

    time++;

    //log_info("on tick %d of %d", time, simulation_ticks);

    // check that the run time hasn't already elapsed and thus needs to be killed
    if ((infinite_run != TRUE) && (time >= simulation_ticks)) {
        //log_info("Simulation complete.\n");

        if (recording_flags > 0) {
            //log_info("updating recording regions");
            recording_finalise();
        }

        // falls into the pause resume mode of operating
        simulation_handle_pause_resume(resume_callback);

        return;

    }

    if (time == 1) {
    	retrieve_header_data();
    	start_processing();
    }
    else if(time == 1000) {
        iobuf_data();
    }

    // trigger buffering_out_mechanism
    if (recording_flags > 0) {
        //log_info("doing timer tick update\n");
        recording_do_timestep_update(time);
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

    if (transmission_region_address[0] > 0) {

    	unsigned int number_of_keys = transmission_region_address[0];
    	key_values = malloc(sizeof(unsigned int) * number_of_keys);
    	log_info("number keys", number_of_keys);
    	for(unsigned int i = 0; i < number_of_keys; i++) {
        	key_values[i] = transmission_region_address[2*i + 1];
            log_info("my key is %d\n", key_values[i]);
    	}

    } else {
        log_error("please fix the application fabric and try again\n");
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
