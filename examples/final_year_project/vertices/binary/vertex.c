//! imports
#include "spin1_api.h"
#include "common-typedefs.h"
#include <data_specification.h>
#include <recording.h>
#include <simulation.h>
#include <debug.h>

/* Debugging mode */
#define DEBUG_1 1
#define DEBUG_2 0
#define DEBUG_3 0
#define DEBUG_4 0
#define DEBUG_START 28000
#define DEBUG_END   28200
/* 0 Default information about cores
 * DEBUG_1 Enables information about messages received and sent
 * DEBUG_2 Debug info on the id distribution algorithm
 * DEBUG_3 Information regarding the construction of the linked list dictionary
 * DEBUG_4 Shows timer ticks
 */

#define RECORD_IDS 0
#define RECORD_LINKED_LIST_LENGTHS 0
#define RECORD_UNIQUE_ITEMS 1
/* 0 record unique ids
 * 1 record the length of the linked list
 */

//amount of milliseconds the application runs
uint runtime = 35000;

/*! multicast routing keys to communicate with neighbours */
uint *key_values;

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
    NEIGHBOUR_INITIAL_STATES,
	DICTIONARY
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

   uint processor_id;
   /* The id of the processor */

   uint num_cols;
   /* number of columns
    * in the original csv file
    */
   uint num_rows;
   /* number of rows
    * in the original csv file
    */
   uint string_size;
   /* number of bytes that
    * are allocated for each individual string
    */
   uint num_string_cols;
   /* Tells you the number of string columns
    * All string columns SHOULD be written to SDRAM first (from python) -
    * then and only then the integer columns
    */
   uint initiate_send;
   /* if 1, vertex will be the first one to send out spike
    * if 0, vertex will wait until spike received
    */
   uint function_id;
   /* holds id of function to be invoked
    * 0 - None
    * 1 - Count number of all data entries within the graph
    * 2 - Builds an index table in every core within the network
    * 3 - Extracts number of unique entries from SDRAM
    */

};

struct header_info header;

///////////////////////////////////////////////////////////////////////////////////////////////////
// DATA ENTRY INDEX - assigns unqiue id's to every data entry within every column           //
///////////////////////////////////////////////////////////////////////////////////////////////////

struct index_info {

	int16_t *id_index;
	/* Holds the unique identifier for each data entry
	 * Example: id_index[1] contains the unique id for the
	 * second data entry within SDRAM
	 * Currently this works only for one column
	 * Length: header.num_rows
	 */
	uint *message;
	/* Holds 4 integers that make up a string
	 * Designed to take a string entry that has been forwarded
	 * by 4 distinct MCPL packages
	 */
	uint  message_id;
	/* Holds the unique id of string above
	 * Takes the id from an incoming MCPL package as well
	 */
	uint  messages_received;
	/* Keeps track of number of MCPL packages received
	 * if messages_received mod 5 = 0, a string data entry and its
	 * id have been received
	 */
    uint  index_complete;
	/* A flag that tells if the index on this vertex is complete
	 * Complete = 1; Incomplete = 0;display_linked_list_size()
	 * Complete means that there are no indices left with value 0
	 */
	uint  max_id;
	/* Tells you the highest id number on this vertex
	 */

};

//declare linked list to be a dictionary
typedef struct node {
	uint *entry;
	char entry_size;
	uint16_t id;
	uint16_t frequency;
	uint16_t global_frequency;
	uint16_t index_start;
	uint16_t index_end;
    struct node *next;
} node_t;

node_t * dictionary;
node_t * current_item;
node_t * end_of_dict;

struct index_info local_index;

//timeout handler
uint time_out_block_start;
uint time_out_block_end;
uint reported_ready;
uint forward_mode_on;
uint current_leader;
uint global_max_id;
uint current_id;
uint in_charge;
uint linked_list_length;
uint sum;

//SDRAM DICTIONARY PARAM
uint TCM_dict_max        = 260;
uint SDRAM_num_elements  = 0;
uint SDRAM_next_slot     = 0;

//STRINGCOMP
uint *current_string;

//MEMORY REGIONS
address_t INPUT_ADDRESS;
address_t DICT_ADDRESS;

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION REFERENCES                                                                           //                                                                  //
///////////////////////////////////////////////////////////////////////////////////////////////////

//Start Operation
address_t return_mem_address(uint partition);
static bool initialise_recording();
static bool initialize(uint32_t *timer_period);

//Manage Operation
void update(uint ticks, uint b);
void c_main();

//Memory Management
void resume_callback();
void iobuf_data();

//Get Data From SDRAM
void retrieve_header_data();

//Write Data to SDRAM
void record_string_entry(uint *int_arr, uint size);
void record_int_entry(uint solution);
void record_unqiue_items(uint start, uint end);

//String Comparison
uint compare_two_strings(uint *string_1, uint size_1, uint *string_2, uint size_2);
uint find_instance_of(uint given_id);
uint verify_signal(uint signal);

//Dictionary Management
node_t *search_tcm_dictionary(uint *string_to_search);
node_t *search_tcm_dict_with_id(uint id_to_search);
void add_item_to_dictionary(uint *given_string, uint index, uint id);
void write_dict_item_to_SDRAM(uint *given_string, uint id);
uint search_sdram_dict_with_id(uint id_to_search);
uint search_sdram_dict(uint *string_to_be_searched);

//Sending Messages
void forward_string();
void send_string(uint data_entry_position);
void send_signal(uint id, uint signal);
void send_function_signal(uint signal, uint entry1, uint entry2);

//Receiving Messages
void send_state(uint payload, uint key);
void receive_data(uint key, uint payload);

//Function Selector
void start_processing();

//Function: Builds Index (Linked list)
void index_initialise_index();
void index_complete_index(uint unique_id, uint start_index);
void index_update_index();
void index_receive(uint payload);
void index_collect_string_message(uint payload);
void index_act_as_subordinate();
void index_act_as_leader();

//Fucntion: Create Histogram
void histogram_receive(uint payload);
void histogram_update();
void histogram_query();
void histogram_record();

//Function: Count Entries
void count_function_start();
void count_function_receive(uint payload);

//Function: Communication Test Leader
void leader_blast();
void leader_collects_reports(uint payload);
void report_to_leader(uint payload);

///////////////////////////////////////////////////////////////////////////////////////////////////
// GENERIC UTILITY FUNCTIONS                                                                     //                                                       //
///////////////////////////////////////////////////////////////////////////////////////////////////

address_t return_mem_address(uint partition){

	address_t address = data_specification_get_data_address();
	address_t data_address =
		data_specification_get_region(partition, address);

	return data_address;

}

void resume_callback() {
    time = UINT32_MAX;
}

void iobuf_data() {

    int* my_string = (int *) &INPUT_ADDRESS[0];
}

node_t *search_tcm_dictionary(uint *string_to_search) {

	node_t * item = dictionary;

	#if defined(DEBUG_3) && (DEBUG_3 == 1)
     	 if((time > DEBUG_START) && (time < DEBUG_END)) {
     		log_info("---------SEARCH-----------");
       	    log_info("| search: %d",item->entry[0]);
       	    log_info("| search: %d",item->entry[1]);
       	    log_info("| search: %d",item->entry[2]);
       	    log_info("| search: %d",item->entry[3]);
       	 }
	#endif

	while(item->frequency != 0) {

	#if defined(DEBUG_3) && (DEBUG_3 == 1)
    	 if((time > DEBUG_START) && (time < DEBUG_END)) {
		    log_info("|                         ");
			log_info("| found: %d",item->entry[0]);
			log_info("| found: %d",item->entry[1]);
			log_info("| found: %d",item->entry[2]);
			log_info("| found: %d",item->entry[3]);
    	 }
		#endif

		if(compare_two_strings(string_to_search,4, item->entry,item->entry_size) == 1) {
			return item;
		}

		item = item->next;

	}

    end_of_dict = item;

	return item;

}

node_t *search_tcm_dict_with_id(uint id_to_search) {

	node_t * item = dictionary;

	while(item->frequency != 0) {
		if(item->id == id_to_search) {
			return item;
		}
		item = item->next;
	}

	end_of_dict = item;

	return item;

}

uint search_sdram_dict_with_id(uint id_to_search){

	//SDRAM DICT NOT IN USE
	if(linked_list_length < TCM_dict_max) {
		return -1;
	}

    uint current_index = 0;

    for(uint i = 0; i < SDRAM_num_elements; i++){

    	if(id_to_search == DICT_ADDRESS[current_index+1]){
    		return current_index;
    	}
    	else{
    		current_index = DICT_ADDRESS[current_index];
    	}

    }

    return -1;

}

uint search_sdram_dict(uint *string_to_be_searched) {

	//SDRAM DICT NOT IN USE
	if(linked_list_length < TCM_dict_max) {
		return -1;
	}

    uint current_index = 0;

    for(uint i = 0; i < SDRAM_num_elements; i++) {

    	uint entry_size = DICT_ADDRESS[current_index] - current_index - 4;

    	for(uint j = 0; j < entry_size; j++){
    		current_string[j] = DICT_ADDRESS[current_index + 4 + j];
    	}

    	if(compare_two_strings(current_string,entry_size,string_to_be_searched,4) == 1) {
    		return current_index;
    	}

		current_index = DICT_ADDRESS[current_index];

    }

    return -1;

}

void add_item_to_dictionary(uint *given_string, uint index, uint id) {

	if(linked_list_length >= TCM_dict_max) {
		write_dict_item_to_SDRAM(given_string, id);
	}
	else{

		node_t * item = end_of_dict;

		//538976288 stands for a 0 entry - if those occur don't store them
		uint count = 0;
		for(uint i = 0; i < 4; i++){
			if(given_string[i] == 538976288) {
				break;
			}
			count++;
		}

		item->entry_size = count;

	    uint *new_entry = malloc(count*sizeof(uint));
	    for(int i = 0; i < count; i++){new_entry[i] = given_string[i];}

	    item->frequency        = 1;
	    item->global_frequency = 1;
		item->entry            = new_entry;
		item->id               = id;
		item->index_start      = index;
		item->index_end        = index+1;

		item->next = malloc(sizeof(node_t));
		item->next->id          = -1;
		item->next->frequency   = 0;
		item->next->index_start = 0;
		item->next->index_end   = 0;

	    end_of_dict = item->next;

		#if defined(DEBUG_3) && (DEBUG_3 == 1)
		 if((time > DEBUG_START) && (time < DEBUG_END)) {
			log_info("----------ADD------------");
			log_info("| ENTRY: %d", item->entry[0]);
			log_info("| FREQ : %d", item->frequency);
			log_info("| START: %d", item->index_start);
			log_info("| END  : %d", item->index_end);
			log_info("| ID   : %d", item->id);
		 }
		#endif

		linked_list_length++;

	}

}

void write_dict_item_to_SDRAM(uint *given_string, uint id) {

	//538976288 stands for a 0 entry - if those occur don't store them
    char entry_size = 0;
	for(uint i = 0; i < 4; i++) {
		if(given_string[i] == 538976288) {
			break;
		}
		entry_size++;
	}

    //POINTER TO NEXT ITEM
    DICT_ADDRESS[SDRAM_next_slot + 0] = SDRAM_next_slot + entry_size + 4;

    //ID
    DICT_ADDRESS[SDRAM_next_slot + 1] = id;

    //FREQUENCY
    DICT_ADDRESS[SDRAM_next_slot + 2] = 1;

    //GLOBAL FREQUENCY
    DICT_ADDRESS[SDRAM_next_slot + 3] = 1;

	for(uint i = 0; i < entry_size; i++) {
		DICT_ADDRESS[SDRAM_next_slot + 4 + i] = given_string[i];
	}

    //UPDATE SDRAM LIST PARAM
    SDRAM_num_elements = SDRAM_num_elements + 1;
    SDRAM_next_slot    = SDRAM_next_slot + entry_size + 4;

}

uint compare_two_strings(uint *string_1, uint size_1, uint *string_2, uint size_2) {

    //compares two strings with each other
	//return 1
	//return 0 if not

	uint bound = size_1;
	if(bound > size_2){bound = size_2;}

    for(uint i = 0; i < bound; i++) {
      if(string_1[i] != string_2[i]){
    	  return 0;
      }
    }

	return 1;

}

uint find_instance_of(uint given_id) {

	uint compare = -1;
	node_t * item = dictionary;

	#if defined(DEBUG_3) && (DEBUG_3 == 1)
	 if((time > DEBUG_START) && (time < DEBUG_END)) {
		log_info("ID TO FIND: %d", given_id);
	 }
	#endif

	while(item->frequency != 0) {

	#if defined(DEBUG_3) && (DEBUG_3 == 1)
		if((time > DEBUG_START) && (time < DEBUG_END)) {
			log_info("|                         ");
			log_info("| found: %d",item->entry[0]);
			log_info("| found: %d",item->entry[1]);
			log_info("| found: %d",item->entry[2]);
			log_info("| found: %d",item->entry[3]);
			log_info("| id   : %d",item->id);
		}
	#endif

		if(item->id == given_id) {
			compare = item->index_start;
			break;
		}

		item = item->next;

	}

	#if defined(DEBUG_3) && (DEBUG_3 == 1)
	   if((time > DEBUG_START) && (time < DEBUG_END)) {

    	log_info("CHECK_1: %d", compare);
    	uint test = -1;
        for(uint i = 0; i < header.num_rows; i++) {
        	if(local_index.id_index[i] == given_id){
        		log_info("CHECK_2: %d", i);
        	}
        }

        if(test == -1){log_info("CHECK_2: %d", test);}

	   }
	#endif

	//not recorded in tcm dictionary -> brute force search
	if(compare == -1) {
		for(uint i = 0; i < header.num_rows; i++){
			if(local_index.id_index[i] == given_id){
				return i;
			}
		}
	}

    return compare;

}

void send_string(uint data_entry_position) {

	//take every column of strings and assign an unique id to each string
	uint start = 7  + 4 * data_entry_position;
	uint end   = 11 + 4 * data_entry_position;
	uint count = 0;

	uint i;
	uint entry[4];

	//current entry
    for(i = start; i < end; i++) {
    	entry[count] = *(&INPUT_ADDRESS[i]);
    	count++;
    }

	//send the first data entry to the next core - 4 spikes
	send_state(entry[0], 2);
	send_state(entry[1], 2);
	send_state(entry[2], 2);
	send_state(entry[3], 2);

	//send the id of that entry
	send_state(local_index.id_index[data_entry_position], 2);

	#if defined(DEBUG_1) && (DEBUG_1 == 1)
	   if((time > DEBUG_START) && (time < DEBUG_END)) {
		log_info("SEND STRING");
		log_info("M1: %d",entry[0]);
		log_info("M2: %d",entry[1]);
		log_info("M3: %d",entry[2]);
		log_info("M4: %d",entry[3]);
		log_info("M5: %d",local_index.id_index[data_entry_position]);
	   }
	#endif

}

void forward_string() {

	send_state(local_index.message[0], 2);
	send_state(local_index.message[1], 2);
	send_state(local_index.message[2], 2);
	send_state(local_index.message[3], 2);
	send_state(local_index.message_id, 2);

	#if defined(DEBUG_1) && (DEBUG_1 == 1)
	   if((time > DEBUG_START) && (time < DEBUG_END)) {
		log_info("FORWARD");
		log_info("M1: %d",local_index.message[0]);
		log_info("M2: %d",local_index.message[1]);
		log_info("M3: %d",local_index.message[2]);
		log_info("M4: %d",local_index.message[3]);
		log_info("M5: %d",local_index.message_id);
	   }
	#endif

}

uint verify_signal(uint signal) {

	uint i;
	for(i = 0; i < 3; i++) {
		if(local_index.message[i] != signal) {
			return 0;
		}
	}

	return 1;

}

void send_signal(uint id, uint signal) {

	//0 processing finished completely
	//1 core ready for other tasks

	//take every column of strings and assign an unique id to each string
	send_state(signal, 2);
	send_state(signal, 2);
	send_state(signal, 2);
	send_state(header.processor_id, 2);
	send_state(id, 2);

	#if defined(DEBUG_1) && (DEBUG_1 == 1)
	   if((time > DEBUG_START) && (time < DEBUG_END)) {
		log_info("SIGNAL");
		log_info("M1: %d",signal);
		log_info("M2: %d",signal);
		log_info("M3: %d",signal);
		log_info("M4: %d",header.processor_id);
		log_info("M5: %d",id);
	   }
	#endif

}

void send_function_signal(uint signal, uint entry1, uint entry2) {

	//signal 0 - leader sends an update
	//signal 1 - leader sends a query

	send_state(signal, 2);
	send_state(signal, 2);
	send_state(signal, 2);
	send_state(entry1, 2);
	send_state(entry2, 2);

	#if defined(DEBUG_1) && (DEBUG_1 == 1)
		if((time > DEBUG_START) && (time < DEBUG_END)) {
			log_info("FUNCTION_SIGNAL");
			log_info("M1: %d",signal);
			log_info("M2: %d",signal);
			log_info("M3: %d",signal);
			log_info("M4: %d",entry1);
			log_info("M5: %d",entry2);
		}
	#endif

}

///////////////////////////////////////////////////////////////////////////////////////////////////                                              //
// START_PROCESSING                                                                              //
///////////////////////////////////////////////////////////////////////////////////////////////////

void start_processing() {

	switch(header.function_id) {

		case 1 :

	         count_function_start();

	         break;

		case 2 :

			 index_initialise_index();
			 break;

		case 3 :

		     break;

	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION INDEX                                                                                //
///////////////////////////////////////////////////////////////////////////////////////////////////

void index_initialise_index() {

	//take every column of strings and assign an unique id to each string
    local_index.id_index          = malloc(sizeof(uint) * header.num_rows);
    local_index.message           = malloc(sizeof(uint) * 4);
    local_index.message_id        = 0;
    local_index.messages_received = 0;
    local_index.index_complete    = 0;
    local_index.max_id            = 0;

    dictionary                    = malloc(sizeof(node_t));
    dictionary->id                = 0;
    dictionary->frequency         = 0;
    dictionary->global_frequency  = 0;
    dictionary->index_start       = 0;
    dictionary->index_end         = 0;
    dictionary->entry_size        = 0;
    end_of_dict                   = dictionary;

	linked_list_length = 0;

    uint current_id = 1;

	uint i,j,start,end,count;
	uint *current_entry;

	for(i = 0; i < header.num_rows; i++) {

		//set index to 0
		local_index.id_index[i] = 0;

		in_charge = 0;
		start = 7  + 4 * i;
		end   = 11 + 4 * i;
		count = 0;

		for(j = start; j < end; j++) {
			current_entry[count] = *(&INPUT_ADDRESS[j]);
			count++;
		}

		node_t *element = search_tcm_dictionary(current_entry);

		#if defined(DEBUG_3) && (DEBUG_3 == 1)
			if((time > DEBUG_START) && (time < DEBUG_END)) {
				log_info("-CURRENT ENTRY: %d", current_entry[0]);
			}
		#endif

		//entry exists in dictionary
		if(element->frequency != 0) {
			element->frequency = (element->frequency) + 1;
			element->index_end = i + 1;
			#if defined(DEBUG_3) && (DEBUG_3 == 1)
			   if((time > DEBUG_START) && (time < DEBUG_END)) {
				log_info("|---------UPDATE------------");
				log_info("| ENTRY: %d", element->entry[0]);
				log_info("| FREQ : %d", element->frequency);
				log_info("| START: %d", element->index_start);
				log_info("| END  : %d", element->index_end);
				log_info("| ID   : %d", element->id);
			   }
			#endif

		}

		//entry does not exist in tcm dictionary
		if(element->frequency == 0) {

			//check if item exists in sdram dictionary
			uint num_str[] = {current_entry[0],current_entry[1],current_entry[2],current_entry[3]};
			uint index_found = search_sdram_dict(num_str);

			//entry exists in sdram
			if(index_found != -1) {

				//FREQUENCY
				DICT_ADDRESS[index_found+2]  = DICT_ADDRESS[index_found+2] + 1;

			}

			if(index_found == -1) {
				add_item_to_dictionary(current_entry,i,0);
			}

		}

	}

		#if defined(DEBUG_3) && (DEBUG_3 == 1)
	   	   if((time > DEBUG_START) && (time < DEBUG_END)) {

	   		   uint test[4];
	   		   test[0] = 1430986784;
	   		   test[1] = 538976288;
	   		   test[2] = 538976288;
	   		   test[3] = 538976288;
	   		   node_t *element = search_tcm_dictionary(test);

	   		   if(element->frequency > 0){
	   			   log_info("ENTRY: %d", element->entry[0]);
	   		   }

	   		   log_info("FREQ : %d", element->frequency);
	   		   log_info("START: %d", element->index_start);
	   		   log_info("END  : %d", element->index_end);
	   		   log_info("ID   : %d", element->id);
	   	   }
	#endif

}

void index_complete_index(uint unique_id, uint start_index) {

	//read the first single entry
	uint i,j;
    uint current_entry[4];
    uint current_id = unique_id;

    for(i = start_index; i < header.num_rows; i++) {

        //check if id has already been assigned
        if(local_index.id_index[i] == 0) {

        	uint start = 7  + 4*i;
        	uint end   = 11 + 4*i;
        	uint count = 0;

        	//current entry
    	    for(j = start; j < end; j++) {
    	    	current_entry[count] = *(&INPUT_ADDRESS[j]);
    	    	count++;
    	    }

    	    node_t *element = search_tcm_dictionary(current_entry);

    		//entry does not exist in tcm dictionary -> goto sdram
    		if(element->frequency == 0) {

    			uint num_str[] = {current_entry[0],current_entry[1],current_entry[2],current_entry[3]};
    			uint index_found = search_sdram_dict(num_str);

        	    if(DICT_ADDRESS[index_found+1] == 0) {

        			//ID
        			DICT_ADDRESS[index_found+1] = current_id;

        			if(local_index.max_id < current_id){
            			local_index.max_id = current_id;
        			}

        			current_id++;

        	    }

        	    local_index.id_index[i] = DICT_ADDRESS[index_found+1];

    		}

    		//element exists in tcm -> no need to go to sdram
    		if(element->frequency != 0) {

        	    if(element->id == 0) {
        	    	element->id = current_id;

        			if(local_index.max_id < current_id){
            			local_index.max_id = current_id;
        			}

        			current_id++;

        	    }

        	    local_index.id_index[i] = element->id;

    		}

        }

    }

    local_index.index_complete = 1;

}

void index_update_index() {

    node_t *element = search_tcm_dictionary(local_index.message);

    //check if element exists in tcm-dictionary
    if(element->id != -1) {

    	//check if element has id 0 assigned to it
        if(element->id == 0) {

        	element->id = local_index.message_id;

			if(local_index.max_id < local_index.message_id){
    			local_index.max_id = local_index.message_id;
			}

        	uint *current_entry;

            for(uint i = element->index_start; i < element->index_end; i++) {

            	uint start = 7  + 4*i;
            	uint end   = 11 + 4*i;
            	uint count = 0;

            	//current entry
        	    for(uint j = start; j < end; j++) {
        	    	current_entry[count] = *(&INPUT_ADDRESS[j]);
        	    	count++;
        	    }

    		    if(compare_two_strings(current_entry,4,element->entry,element->entry_size) == 1) {
    		    	local_index.id_index[i] = element->id;
    		    }

            }

        }

    }

    //check if element exists in tcm-dictionary
    if(element->frequency == 0) {

        uint index_found = search_sdram_dict(local_index.message);

		//entry exists in sdram
		if(index_found != -1) {

		    if(DICT_ADDRESS[index_found+1] == 0) {

				//ID
				DICT_ADDRESS[index_found+1] = local_index.message_id;

				if(local_index.max_id < local_index.message_id){
	    			local_index.max_id = local_index.message_id;
				}

	        	uint *current_entry;

	            for(uint i = 0; i < header.num_rows; i++) {

	            	uint start = 7  + 4*i;
	            	uint end   = 11 + 4*i;
	            	uint count = 0;

	            	//current entry
	        	    for(uint j = start; j < end; j++) {
	        	    	current_entry[count] = *(&INPUT_ADDRESS[j]);
	        	    	count++;
	        	    }

	    			uint num_str[] = {current_entry[0],current_entry[1],current_entry[2],current_entry[3]};
	    		    if(compare_two_strings(num_str,4,local_index.message,4) == 1) {
	    		    	local_index.id_index[i] = element->id;
	    		    }

	            }

		    }

		}

    }//if element->frequency == 0

}

void index_receive(uint payload) {

	local_index.messages_received++;

	//collect up to 5 messages
	if(local_index.messages_received % 5 != 0) {
		index_collect_string_message(payload);
	}
	else {

		#if defined(DEBUG_2) && (DEBUG_2 == 1)
			if((time > DEBUG_START) && (time < DEBUG_END)) {
				log_info("RECEIVED: %d", payload);
				log_info("------------------");
		}
		#endif

		local_index.message_id = payload;

		//RECEIVE 3-3-3-3 -> change function ID
		if(verify_signal(3) == 1) {
			header.function_id = local_index.message_id;
			send_state(-1,2);
		}
		else {

			//Select Mode of Operation
			if(in_charge == 0) {index_act_as_subordinate();}

			if(in_charge == 1) {index_act_as_leader();}

		}

	}

}

void index_collect_string_message(uint payload){

	local_index.message[(local_index.messages_received % 5) - 1] = payload;
	#if defined(DEBUG_2) && (DEBUG_2 == 1)
	   if((time > DEBUG_START) && (time < DEBUG_END)) {
		   log_info("RECEIVED: %d", payload);
	   }
	#endif

}

void index_act_as_leader() {

	if(verify_signal(1) == 1) {

		#if defined(DEBUG_2) && (DEBUG_2 == 1)
		   if((time > DEBUG_START) && (time < DEBUG_END)) {
			   log_info("SPARTA: %d", local_index.message_id);
			   log_info("received: %d", local_index.message_id);
			   log_info("max_id:   %d", local_index.max_id);
		   }
		#endif

		if(local_index.message_id <= local_index.max_id) {
			send_string(find_instance_of(local_index.message_id));
		}
		else {
			in_charge = 0;
			send_signal(local_index.message_id,0);

			#if defined(DEBUG_2) && (DEBUG_2 == 1)
				if((time > DEBUG_START) && (time < DEBUG_END)) {
					log_info("OLD LEADER: %d",header.processor_id);
					log_info("max_id: %d", local_index.message_id);
				}
			#endif
		}
	}

}

void index_act_as_subordinate() {

	//ignore 1-1-1 messages
	if(verify_signal(1) == 1) {return;}

	//0-0-0-0 -> reassign leader
	if(verify_signal(0) == 1) {

		if(local_index.message[3]+1 == header.processor_id){

			in_charge = 1; //-> you are the new leader

			//Make sure that the index is complete
			int zeros_exist = find_instance_of(0); //find first occurence of 0 index

			//log_info("DEEZNATS: %d", zeros_exist);

			//zeros exist
			if(zeros_exist != -1) {
				index_complete_index(local_index.message_id, zeros_exist);
				send_string(zeros_exist);
			}

			//zeros don't exist
			if(zeros_exist == -1) {
				in_charge = 0;
				local_index.index_complete = 1;
				send_signal(local_index.message_id,0);
			}

			//Recording information
			#if defined(RECORD_LINKED_LIST_LENGTHS) && (RECORD_LINKED_LIST_LENGTHS == 1)
				record_int_entry(header.num_rows);
				record_int_entry(linked_list_length);
			#endif

			#if defined(RECORD_IDS) && (RECORD_IDS == 1)
				for(uint i = 0; i < header.num_rows; i++) {
					record_int_entry(local_index.id_index[i]);
				}
			#endif

		}

	}

	//normal string message
	if(verify_signal(0) == 0) {

		if(local_index.index_complete == 0) {
			index_update_index();
		}

		send_state(-1, 2); //report ready

		#if defined(DEBUG_1) && (DEBUG_1 == 1)
			if((time > DEBUG_START) && (time < DEBUG_END)) {
				log_info("SEND ACKNOWLEDGEMENT: -1");
				log_info("Index complete: %d", local_index.index_complete);
				log_info("Core in charge: %d", in_charge);
				log_info("Processor id: %d", header.processor_id);
			}
		#endif

	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION HISTOGRAM                                                                            //
///////////////////////////////////////////////////////////////////////////////////////////////////

void histogram_receive(uint payload) {

	local_index.messages_received++;

	//collect up to 5 messages
	if(local_index.messages_received % 5 != 0) {
		local_index.message[(local_index.messages_received % 5) - 1] = payload;
	}
	else {

		local_index.message_id = payload;

		if(verify_signal(0) == 1){histogram_update();}
		if(verify_signal(1) == 1){histogram_query();}
		if(verify_signal(2) == 1){histogram_record();}

	}

}

void histogram_update(){

	node_t *found = search_tcm_dict_with_id(local_index.message_id);

    //if in tcm dictionary
	if(found->frequency != 0) {
		found->global_frequency = local_index.message[3];
	}

    //check if in sdram if need be
    if(found->frequency == 0) {

        uint index_found = search_sdram_dict_with_id(local_index.message_id);

		//entry exists in sdram
		if(index_found != -1) {
			DICT_ADDRESS[index_found+3] = local_index.message[3];
		}

    }

	send_state(-1, 2);

	#if defined(DEBUG_1) && (DEBUG_1 == 1)
		if((time > DEBUG_START) && (time < DEBUG_END)) {
			log_info("SEND ACKNOWLEDGEMENT: -1");
		}
	#endif

}

void histogram_query(){

	node_t *found = search_tcm_dict_with_id(local_index.message_id);

    //if in tcm dictionary
	if(found->frequency != 0) {
		send_state(found->frequency, 2);

		#if defined(DEBUG_1) && (DEBUG_1 == 1)
			if((time > DEBUG_START) && (time < DEBUG_END)) {
				log_info("SEND FREQUENCY: %d", found->frequency);
			}
		#endif

	}

    //check if in sdram if need be
    if(found->frequency == 0) {

        uint index_found = search_sdram_dict_with_id(local_index.message_id);

		//entry exists in sdram
		if(index_found != -1) {

			//FREQUENCY
			send_state(DICT_ADDRESS[index_found+2], 2);

			#if defined(DEBUG_1) && (DEBUG_1 == 1)
				if((time > DEBUG_START) && (time < DEBUG_END)) {
					log_info("SEND FREQUENCY: %d", DICT_ADDRESS[index_found+2]);
				}
			#endif

		}

		//entry does not exist in sdram
		if(index_found == -1) {
			send_state(0, 2);

			#if defined(DEBUG_1) && (DEBUG_1 == 1)
				if((time > DEBUG_START) && (time < DEBUG_END)) {
					log_info("SEND FREQUENCY: %d", 0);
				}
			#endif

		}

    }


}

void histogram_record(){

	uint the_leader = local_index.message[3];
	if(the_leader == header.processor_id) {
		uint start_id  = local_index.message_id;

		if(start_id <= local_index.max_id){
			record_unqiue_items(start_id,local_index.max_id);
			send_state(local_index.max_id+1,2);
		}

		if(start_id > local_index.max_id){
			send_state(start_id,2);
		}

	#if defined(DEBUG_1) && (DEBUG_1 == 1)
		if((time > DEBUG_START) && (time < DEBUG_END)) {
			log_info("SEND RECORDED: %d", local_index.max_id);
		}
	#endif

	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION COUNT                                                                                //
///////////////////////////////////////////////////////////////////////////////////////////////////

void count_function_start() {

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

	#if defined(DEBUG_1) && (DEBUG_1 == 1)
	   if((time > DEBUG_START) && (time < DEBUG_END)) {
		   log_info("--package arrived-- %d", payload);
	   }
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
			 histogram_receive(payload);
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

	INPUT_ADDRESS = return_mem_address(INPUT_DATA);
	DICT_ADDRESS = return_mem_address(DICTIONARY);

    //header data that contains:
    //- The data description
    //- Processing instructions
    header.processor_id    = INPUT_ADDRESS[0];
    header.num_cols        = INPUT_ADDRESS[1];
	header.num_rows        = INPUT_ADDRESS[2];
	header.string_size     = INPUT_ADDRESS[3];
	header.num_string_cols = INPUT_ADDRESS[4];
	header.initiate_send   = INPUT_ADDRESS[5];
	header.function_id     = INPUT_ADDRESS[6];

	reported_ready  = 0;
	forward_mode_on = 0;
	global_max_id   = 0;
	in_charge       = 0;
	current_leader  = 0;

	current_string = malloc(4*sizeof(uint));

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

void record_unqiue_items(uint start, uint end) {
	record_tcm_dict(start, end);
	record_sdram_dict(start, end);
}

void record_tcm_dict(uint start, uint end) {

	node_t *item = dictionary;

	while(item->frequency != 0) {

		if(item->id < start) {
			item = item->next;
		}

		if(item->id >= start) {

			if(item->id <= end) {
				log_info("ID: %d",item->id);
				record_string_entry(item->entry,item->entry_size);
				record_int_entry(item->global_frequency);
				item = item->next;

			}

		}

	}

}

void record_sdram_dict(uint start, uint end) {

    uint current_index = 0;

    for(uint i = 0; i < SDRAM_num_elements; i++) {

    	uint *result;
    	uint entry_size = DICT_ADDRESS[current_index] - current_index - 4;

    	for(uint j = 0; j < entry_size; j++){
    		result[j] = DICT_ADDRESS[current_index + 4 + j];
    	}

    	uint current_id = DICT_ADDRESS[current_index+1];

    	if(current_id >= start) {

    		if(current_id <= end) {
				log_info("ID: %d",current_id);
        		record_string_entry(result,entry_size);
        		record_int_entry(DICT_ADDRESS[current_index + 3]);
    		}

    	}

		current_index = DICT_ADDRESS[current_index];

    }

}

void record_string_entry(uint *int_arr, uint size) {

	//convert the array of [size] integers to a 4*[size] char array
	unsigned char buffer[header.string_size];

	uint num_ints = header.string_size/4;

	uint i = 0;
	for(i = 0; i < size; i++) {
      buffer[num_ints*i + 0] = (int_arr[i] >> 24) & 0xFF;
	  buffer[num_ints*i + 1] = (int_arr[i] >> 16) & 0xFF;
	  buffer[num_ints*i + 2] = (int_arr[i] >> 8) & 0xFF;
	  buffer[num_ints*i + 3] =  int_arr[i] & 0xFF;
	}


	if(size < header.string_size/4) {

		uint remain = (header.string_size/4) - size;

		for(uint k = i; k < header.string_size/4; k++) {
			buffer[num_ints*k + 0] = 32;
			buffer[num_ints*k + 1] = 32;
			buffer[num_ints*k + 2] = 32;
			buffer[num_ints*k + 3] = 32;
		}

	}

    //log_info("String Entry : %s", buffer);

    //record the data entry in the first recording region (which is OUTPUT)
    bool recorded = recording_record(0, buffer, header.string_size * sizeof(unsigned char));

}

void record_int_entry(uint solution) {

	char result[10];
	itoa(solution,result,10);

    //uintegers take 10 chars if represented as char arrays
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

    if(DEBUG_1 == 1 || DEBUG_2 == 1 || DEBUG_3 == 1 || DEBUG_4 == 1){
    	if((time > DEBUG_START) && (time < DEBUG_END)) {
    		 log_info("on tick %d of %d", time, simulation_ticks);
    	}
    }

    // check that the run time hasn't already elapsed and thus needs to be killed
    if ((infinite_run != TRUE) && (time >= simulation_ticks)) {
        //log_info("Simulation complete.\n");

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
    }
    else if(time == runtime) {
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
	address_t recording_region = return_mem_address(OUTPUT_DATA);
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

    	uint number_of_keys = transmission_region_address[0];
    	key_values = malloc(sizeof(uint) * number_of_keys);
    	log_info("number keys", number_of_keys);
    	for(uint i = 0; i < number_of_keys; i++) {
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
