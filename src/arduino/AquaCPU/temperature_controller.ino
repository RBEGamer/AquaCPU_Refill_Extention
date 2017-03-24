/*
1wire eingang temeprature setzt

output setzten



*/
#define temp_control_ram_start_index 0
#define temp_control_max_temp 30
#define temp_control_min_temp 0
#define temp_control_default_temp 25
#define temp_control_default_hysterese 1 //zb 1 grad


#define temp_control_max_temp_controllers 8 //depend on the datatype
uint8_t current_temp_controllers = 0; 
struct temp_control_data{
  uint16_t current_temp;  
  uint16_t set_temp;  
  bool enabled;
  uint16_t output_channel_id;
};

temp_control_data temp_controllers[temp_control_max_temp_controllers];


void temp_control_setup(){
/**/  



  
}

void temp_control_update(){
  
  
  
  }








