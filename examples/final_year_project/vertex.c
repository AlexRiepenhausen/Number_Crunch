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
static uint32_t current_payload;

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

//global variables holding the data pointers
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
   unsigned int flag;
   /* flag has 32bits
    * each bit can be 0 or 1
    * 0 stands for string data
    * 1 stands for integer data
    * Example: if the first bit is 0,
    *          the first columns holds string data
    * Warning: There should be no more than 32 columns
    */
   unsigned int initiate_send;
   /* if 1, vertex will be the first one to send out spike
    * if 0, vertex will wait until spike received
    */

};

//Holds header information globally
struct header_info header;

void send_state() {

    // reset for next iteration
    alive_states_recieved_this_tick = 0;
    dead_states_recieved_this_tick = 0;

    // send my new state to the simulation neighbours
    log_debug("sending my state of %d via multicast with key %d",
              my_state, my_key);
    while (!spin1_send_mc_packet(my_key, my_state, WITH_PAYLOAD)) {
        spin1_delay_us(1);
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

   //forward the message to the next vertex (ring)
   send_state();

}

void iobuf_data(){
    address_t address = data_specification_get_data_address();
    address_t data_address =
        data_specification_get_region(INPUT_DATA, address);

    log_info("Data address is %08x", data_address);

    int* my_string = (int *) &data_address[0];
    log_info("Data read is: %s", my_string);
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

    if (recorded) {
        log_info("Vertex data recorded successfully!");
    } else {
        log_info("Vertex was not recorded...");
    }

}

void retrieve_data(){

    log_info("Retrieving data...");
    uint chip = spin1_get_chip_id();
    uint core = spin1_get_core_id();
    log_info("Issuing 'Vertex' from chip %d, core %d", chip, core);

    //access the partition of the SDRAM where input data is stored
    address_t address = data_specification_get_data_address();
    address_t data_address =
        data_specification_get_region(INPUT_DATA, address);

 /* get the header data -
  * the global header struct provides
  * enough information to retrieve any entry
  * at any point in time anywhere from SDRAM
  */

    header.num_cols      = data_address[0];
	header.num_rows      = data_address[1];
	header.string_size   = data_address[2];
	header.flag          = data_address[3];
	header.initiate_send = data_address[4];

	//log this information to iobuf
	log_info("Num_cols: %d", header.num_cols );
	log_info("Num_rows: %d", header.num_rows);
	log_info("string_size: %d", header.string_size);
	log_info("flag : %d", header.flag);
	log_info("initate_send : %d", header.initiate_send);

	//send the first MCPL package if initiate is TRUE
	if(header.initiate_send == 1){
		send_state();
	}

}

void record_data() {

    //access the partition of the SDRAM where input data is stored
    address_t address = data_specification_get_data_address();
    address_t data_address =
        data_specification_get_region(INPUT_DATA, address);

    unsigned int i = 0;
    unsigned int entry1[4];
    for(i = 5; i < 9; i++){
    	entry1[i-5] = *(&data_address[i]);
    }

    //record the data entry in the first recording region (which is OUTPUT)
    record_string_entry(entry1);
}

//! \brief Initialises the recording parts of the model
//! \return True if recording initialisation is successful, false otherwise
static bool initialise_recording(){
    address_t address = data_specification_get_data_address();
    address_t recording_region = data_specification_get_region(
        OUTPUT_DATA, address);
    bool success = recording_initialize(recording_region, &recording_flags);
    log_info("Recording flags = 0x%08x", recording_flags);
    return success;
}

void resume_callback() {
    time = UINT32_MAX;
}

/****f*
 *
 * SUMMARY
 *
 * SYNOPSIS
 *  void update (uint ticks, uint b)
 *
 * SOURCE
 */
void update(uint ticks, uint b) {
    use(b);
    use(ticks);

    time++;

    log_info("on tick %d of %d", time, simulation_ticks);

    // check that the run time hasn't already elapsed and thus needs to be
    // killed
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
    	retrieve_data();
        record_data();
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

static bool initialize(uint32_t *timer_period) {
    log_info("Initialise: started\n");

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

/****f*
 *
 * SUMMARY
 *  This function is called at application start-up.
 *  It is used to register event callbacks and begin the simulation.
 *
 * SYNOPSIS
 *  int c_main()
 *
 * SOURCE
 */

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
