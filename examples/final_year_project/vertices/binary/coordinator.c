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
#define DEBUG_START 100
#define DEBUG_END   150

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
uint runtime = 3000;

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

//TIMEOUT HANDLER
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
uint TCM_dict_exhausted  = 0;
uint SDRAM_num_elements  = 0;
uint SDRAM_next_slot     = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION REFERENCES                                                                           //                                                                  //
///////////////////////////////////////////////////////////////////////////////////////////////////

//Start Operation
static bool initialise_recording();
static bool initialize(uint32_t *timer_period);

//Manage Operation
void update(uint ticks, uint b);
void c_main();

//Memory Management
address_t return_mem_address(uint partition);
void resume_callback();
void iobuf_data();

//Get Data From SDRAM
void retrieve_header_data();

//Write Data to SDRAM
void record_string_entry(uint *int_arr, uint size);
void record_int_entry(uint solution);
void record_unqiue_items(uint start, uint end);
void record_tcm_dict(uint start, uint end);
void record_sdram_dict();

//String Comparison
uint compare_two_strings(uint *string_1, uint size_1, uint *string_2, uint size_2);
uint find_instance_of(uint given_id);
uint verify_signal(uint signal);

//Dictionary Management
node_t *search_dictionary(uint *string_to_search);
void add_item_to_dictionary(uint *given_string, uint index, uint id);
void write_dict_item_to_SDRAM(uint *given_string, uint id);
uint search_dict_item_in_SDRAM(uint id_to_search);

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
void index_receive(uint payload);
void index_leader_awaits_reports(uint payload);
void index_manage_proxy_leader(uint payload);
void index_collect_string_message(uint payload);

//Fucntion: Create Histogram
void histogram_start_function();
void histogram_leader_next_step();
void histogram_synchronise_entries(uint payload);

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

    address_t data_address = return_mem_address(INPUT_DATA);
    int* my_string = (int *) &data_address[0];
}

node_t *search_dictionary(uint *string_to_search) {

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

node_t *search_dictionary_with_id(uint id_to_search) {

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

void add_item_to_dictionary(uint *given_string, uint index, uint id) {

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

void write_dict_item_to_SDRAM(uint *given_string, uint id) {

	//538976288 stands for a 0 entry - if those occur don't store them
    char entry_size = 0;
	for(uint i = 0; i < 4; i++) {
		if(given_string[i] == 538976288) {
			break;
		}
		entry_size++;
	}

    address_t data_address = return_mem_address(DICTIONARY);

    //UPDATE SDRAM LIST PARAM
    SDRAM_num_elements = SDRAM_num_elements + 1;
    SDRAM_next_slot    = SDRAM_next_slot + entry_size + 3;

    //POINTER TO NEXT ITEM
	bool recorded = recording_record(DICTIONARY,SDRAM_next_slot,sizeof(uint));

	//ID
	recorded = recording_record(DICTIONARY,id, sizeof(uint));

	//FREQUENCY
	recorded = recording_record(DICTIONARY, 1, sizeof(uint));

	//GLOBAL_FREQUENCY
	recorded = recording_record(DICTIONARY, 1, sizeof(uint));

	for(uint i = 0; i < entry_size; i++) {
	    bool recorded = recording_record(DICTIONARY, given_string[i], sizeof(uint));
	}

}

uint search_dict_item_in_SDRAM(uint id_to_search){

    address_t data_address = return_mem_address(DICTIONARY);

    uint current_index = 1;

    for(uint i = 0; i < SDRAM_num_elements; i++){

    	if(id_to_search = data_address[current_index]){
    		return current_index - 1;
    	}
    	else{
    		current_index = data_address[current_index - 1];
    	}
    }

    return 0;

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

    return compare;

}

void send_string(uint data_entry_position) {

	//take every column of strings and assign an unique id to each string
    address_t data_address = return_mem_address(INPUT_DATA);

	uint start = 7  + 4 * data_entry_position;
	uint end   = 11 + 4 * data_entry_position;
	uint count = 0;

	uint i;
	uint entry[4];

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

			 //record the index information
			 #if defined(RECORD_IDS) && (RECORD_IDS == 1)
				 for(uint i = 0; i < header.num_rows; i++) {
					record_int_entry(local_index.id_index[i]);
				 }
			 #endif

			 global_max_id = 1;
			 send_string(0); //-> first entry
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

    address_t data_address = return_mem_address(INPUT_DATA);

	uint i,j,start,end,count;
	uint *current_entry;

	//read the first single entry
	for(i = 0; i < header.num_rows; i++) {

		start = 7  + 4*i;
		end   = 11 + 4*i;
		count = 0;

		for(j = start; j < end; j++) {
			current_entry[count] = *(&data_address[j]);
			count++;
		}

		node_t *element = search_dictionary(current_entry);

		//entry exists in dictionary
		if(element->frequency != 0) {
			local_index.id_index[i] = element->id;
			element->frequency = (element->frequency) + 1;
			element->global_frequency = element->frequency;
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

		//entry does not exist in dictionary
		if(element->frequency == 0) {
			local_index.id_index[i] = current_id;
			add_item_to_dictionary(current_entry,i,current_id);
			local_index.max_id = current_id;
			current_id++;
		}

	}

	//all data entries have a non zero index assigned to them
	local_index.index_complete = 1;

	#if defined(RECORD_LINKED_LIST_LENGTHS) && (RECORD_LINKED_LIST_LENGTHS == 1)
		record_int_entry(header.num_rows);
		record_int_entry(linked_list_length);
	#endif


	#if defined(DEBUG_3) && (DEBUG_3 == 1)
	   if((time > DEBUG_START) && (time < DEBUG_END)) {

		   uint test[4];
		   test[0] = 1430986784;
		   test[1] = 538976288;
		   test[2] = 538976288;
		   test[3] = 538976288;
		   node_t *element = search_dictionary(test);

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

void index_receive(uint payload) {

	//Case 1: Leader waiting for reports
	if(forward_mode_on == 0) {
		index_leader_awaits_reports(payload);
	}

	//Case 2: Leader function as a proxy for another leader
	if(forward_mode_on == 1) {
		index_manage_proxy_leader(payload);
	}

}

void index_leader_awaits_reports(uint payload) {

	//15 cores have to report - if in proxy mode only 14
	uint cores_to_report = 15;
	if(current_leader != header.processor_id) {
		cores_to_report = 14;
	}

	//see if everyone is ready
	if(payload == -1){reported_ready++;}
	if(reported_ready == cores_to_report){

		reported_ready = 0;

		//if the coordinator is not in charge anymore
		if(current_leader % 16 != 0) {
			global_max_id++;
			send_signal(global_max_id,1);
			forward_mode_on = 1;
		}

		//if the coordinator is still in charge
		if(current_leader % 16 == 0) {

			global_max_id++;
			if(global_max_id <= local_index.max_id) {
				send_string(find_instance_of(global_max_id));
			}
			else {
				send_signal(global_max_id,0);
				current_leader = 1; //-> next core becomes the leader
				forward_mode_on = 1;
			}

		}

	}

}

void index_manage_proxy_leader(uint payload) {

	local_index.messages_received++;

	//collect up to 5 messages
	if(local_index.messages_received % 5 != 0) {
		index_collect_string_message(payload);
	}
	else{

	#if defined(DEBUG_2) && (DEBUG_2 == 1)
	   if((time > DEBUG_START) && (time < DEBUG_END)) {
		   log_info("RECEIVED: %d", payload);
		   log_info("------------------");
	   }
	#endif

		local_index.message_id = payload;

		if(verify_signal(0) == 0) {
			forward_string();
			forward_mode_on = 0;
		}

		if(verify_signal(0) == 1) {

			current_leader++;
			if(current_leader <= 15) {

				forward_string(); //-> next core becomes the leader
				forward_mode_on = 1;

			#if defined(DEBUG_2) && (DEBUG_2 == 1)
				if((time > DEBUG_START) && (time < DEBUG_END)) {
					log_info("max_id: %d", local_index.message_id);
					log_info("leader: %d", local_index.message[3]);
				}
			#endif

			}

			//kickstart retrieving unique id's result -> counting
		    #if defined(RECORD_UNIQUE_ITEMS) && (RECORD_UNIQUE_ITEMS == 1)
				if(current_leader == header.processor_id + 16){
					log_info("END OF ID ASSIGNMENT PROCESS");
					header.function_id = 3;
					histogram_start_function();
				}
			#endif

		}

	}

}

void index_collect_string_message(uint payload) {

	local_index.message[(local_index.messages_received % 5) - 1] = payload;

	#if defined(DEBUG_2) && (DEBUG_2 == 1)
		if((time > DEBUG_START) && (time < DEBUG_END)) {
			log_info("RECEIVED: %d", payload);
		}
	#endif

	//temporary fix for a glitch - band aid
	if(payload == -1) {
		local_index.messages_received--;
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION HISTOGRAM                                                                            //
///////////////////////////////////////////////////////////////////////////////////////////////////

void histogram_start_function() {

    time_out_block_start          = 0;
    time_out_block_end            = 0;

	local_index.messages_received = 0;
	forward_mode_on               = 0;
	in_charge                     = 1;
	sum                           = 0;
	current_id                    = 1;
	current_leader                = header.processor_id;

	current_item = dictionary;

	//-> tell subordinates to invoke function 3
	send_function_signal(3,0,3);

}

void histogram_receive(uint payload) {

	if(forward_mode_on == 0) {

		//timeout handler
		if(reported_ready == 0){time_out_block_start = time;}
		if(reported_ready > 12){time_out_block_end   = time;}

		//leader performing next step
		if(payload == -1){reported_ready++;}
		if(reported_ready == 15) {histogram_leader_next_step();}

	}

	//Case 2: You are the leader and collecting information
	if(forward_mode_on == 1) {
		histogram_synchronise_entries(payload);
	}

}

void histogram_leader_next_step() {

	reported_ready = 0;

    time_out_block_start = 0;
    time_out_block_end   = 0;

    //all info synchronised - tell everyone to record
	if(current_id > global_max_id) {

		record_unqiue_items(1,local_index.max_id);
		current_leader++;
		send_function_signal(2, current_leader, local_index.max_id + 1); //RECORD
		forward_mode_on = 1;
		return;
	}

	forward_mode_on = 1;
	send_function_signal(1, 0, current_id);

}

void histogram_synchronise_entries(uint payload) {

	if(current_id > global_max_id) {

		if(payload == -1){return;}

		current_leader++;
		send_function_signal(2, current_leader, payload); //RECORD
		return;
	}

	if(payload != -1){
		reported_ready++;
		sum = sum + payload;
	}

	if(reported_ready == 15) {

		reported_ready = 0;
		forward_mode_on = 0;

		//update leaders dictionary if necessary
		if(current_id <= local_index.max_id){

			node_t *found = search_dictionary_with_id(current_id);
			if(found->frequency != 0) {
				found->global_frequency = found->frequency + sum;
			}

		}

		send_function_signal(0, sum, current_id);
		current_id++;
		sum = 0;

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

    //access the partition of the SDRAM where input data is stored
    address_t data_address = return_mem_address(INPUT_DATA);

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

void record_unqiue_items(uint start, uint end) {
	record_tcm_dict(start, end);
	//record_sdram_dict();
}

void record_tcm_dict(uint start, uint end) {

	node_t *item = dictionary;

	while(item->frequency != 0) {

		if(item->id < start) {
			item = item->next;
		}

		if(item->id >= start) {

			if(item->id <= end) {
				record_string_entry(item->entry,item->entry_size);
				record_int_entry(item->global_frequency);
				item = item->next;
			}

			if(item->id > end) {
				return;
			}

		}

	}

}

void record_sdram_dict() {

    address_t data_address = return_mem_address(DICTIONARY);

    uint current_index = 0;

    for(uint i = 0; i < SDRAM_num_elements; i++) {

    	uint *result;
    	uint entry_size = data_address[current_index] - current_index - 4;

    	for(uint j = 0; j < entry_size; j++){
    		result[j] = data_address[current_index + 4 + j];
    	}

		record_string_entry(result,entry_size);
		record_int_entry(data_address[current_index + 3]);

		current_index = data_address[current_index];

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

    if(header.function_id == 3) {
    	if(time_out_block_start != 0 && time_out_block_end != 0) {

    		log_info("timeout");
    		uint time_taken  = time_out_block_end - time_out_block_start;
    		uint time_passed = time - time_out_block_end;

    		if(time_taken == 0){
    			if(time_passed == 5){histogram_leader_next_step();}
    		}

    		if(time_taken != 0){
    			if(time_passed == time_taken * 5){histogram_leader_next_step();}
    		}

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

    address_t address = data_specification_get_data_address();

    address_t recording_region = data_specification_get_region(OUTPUT_DATA, address);
    bool success = recording_initialize(recording_region, &recording_flags);

    recording_region = data_specification_get_region(DICTIONARY, address);
    success = recording_initialize(recording_region, &recording_flags);

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
