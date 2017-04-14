#include <iostream>


/*
RECIEVE CAN MESSAGES WITH A FILTER OR ALL


//DOWNLOAD FILTER FILE WITH DATATYPES
EINE CSV FILE FÜR JEDE EXTENTION MIT DEN MESSAGES



*/



/*
-> move simple logger to seperate class with singleton
->check at message file parsing the the string only contains numbers inkl dot not
*/


#define VERSION "0.9a"
#define PROGRAM_NAME "AquaCPU_SMS_GATEWAY"
#define AUTHOR "Marcel Ochsendorf"
#define AUTHOR_EMAIL "marcel.ochsendorf@gmail.com"
#define LICENCE "GNU V3"


#include <iostream>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <endian.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include <stdlib.h>
#include <thread>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>
#include <cstring> //for strstr <3
#include <signal.h>   
#include <vector>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include "serialib.h"
#include "ini_parser.hpp"
#include <wiringPi.h>


namespace simple_logger {

#define log_error(x) log(x,LOG_TYPE::ERROR)
#define log_info(x) log(x, LOG_TYPE::INFO);
#define log_warning(x) log(x, LOG_TYPE::WARNING);

	std::ofstream log_file;
	bool file_correct = false;

	enum LOG_TYPE
	{
		ERROR = 0,
		INFO = 1,
		WARNING = 2
	};
	bool start_logger(const char* _path) {
		log_file.open(_path, std::ios_base::app | std::ios_base::out);
		file_correct = log_file.good();
		log_file << "<b>------------------ START NEW LOG ------------------</b><br>" << "\n";
		log_file.flush();
		return file_correct;
	}

	void log(const std::string _reas, LOG_TYPE _type = LOG_TYPE::ERROR) {
		if (!file_correct) { return; }
		std::string log_entry = "";
		log_entry.append("->");
		switch (_type)
		{
		case LOG_TYPE::ERROR:log_entry.append("<b>ERROR</b>"); break;
		case LOG_TYPE::INFO:log_entry.append("INFO"); break;
		case LOG_TYPE::WARNING:log_entry.append("WARNING"); break;
		default:log_entry.append("<b>UNKNOWN_TYPE</b>"); break;
		}
		log_entry.append(" : ");
		log_entry.append(_reas);
		log_file << log_entry << "<br>\n";
		log_file.flush();
	}

	void stop_log() {
		if (!file_correct) { return; }
		log_file.close();
	}

}

enum ENDIANNESS
{
	LITTE = 0,
	BIG = 1
};
enum CAN_DATA_TYPES
{
	BOOL_1 = 0,
	INT_2 = 1,
	LONG_4 = 2,
	FLOAT_3 = 3,
};
struct CAN_MESSAGES_STRUCT
{
	long extention_id;
	long listen_can_id;
	CAN_DATA_TYPES type;
	std::string message;
	ENDIANNESS byte_order;

};

union DATA_UNION
{
	__u8 byt[8];
	float float_3;
	int int_2;
	long long_4;
	bool bool_1;

};



//holds a can fraem with converted datatype
struct CAN_PROCECCED_FRAME_STRUCT
{
	long can_id;
	DATA_UNION data;
	CAN_DATA_TYPES type;
};

struct UART_MESSAGES
{
	std::string to_send;
	std::string expected;
	bool send;
	bool noCR;
};
// ------------------ GLOBALS -------------------------------------

sig_atomic_t signaled = 0; //FOR SINAL HANDLING
std::vector<CAN_MESSAGES_STRUCT> listen_can_messages; //PARSED CAN LISTEN MESSAGES FORM  CSV FILE
int soc;
int read_can_port;
//UART
int uart0_filestream = -1;
serialib LS;                                                            // Object of the serialib class
int Ret;             //returns the recieved serial chars                                                   // Used for return values
char Buffer[128]; //for the serial recieve

std::vector<std::string> sms_messages;
// ------------------- END GLOBALS -------------------------------


//STRING HELPER FUNC
std::string RemoveChar(std::string _s, const char _c)
{
	std::string result;
	for (size_t i = 0; i < _s.size(); i++)
	{
		char currentChar = _s[i];
		if (currentChar != _c)
			result += currentChar;
	}
	return result;
}
int count_chars_in_string(const std::string& _s, const char _c) {
	int count = 0;
	for (int i = 0; i < _s.size(); i++)
		if (_s[i] == _c) count++;
	return count;
}
bool check_for_only_numbers(const std::string& s) {
	//	char* p;
	//	strtod(s.c_str(), &p);
	//	return *p == 0;
	// return !s.empty() && s.find_first_not_of("-.0123456789") == std::string::npos;
	return true;
}



//CAN FUNC

void process_can_frame(can_frame& _frame) {

	for (size_t i = 0; i < listen_can_messages.size(); i++)
	{
		if (_frame.can_id == listen_can_messages[i].listen_can_id) {
			std::cout << listen_can_messages[i].message << std::endl;
				sms_messages.push_back(listen_can_messages[i].message);
			DATA_UNION pdata;
			if (listen_can_messages[i].byte_order == ENDIANNESS::BIG) {
				pdata.byt[0] = _frame.data[7];
				pdata.byt[1] = _frame.data[6];
				pdata.byt[2] = _frame.data[5];
				pdata.byt[3] = _frame.data[4];
				pdata.byt[4] = _frame.data[3];
				pdata.byt[5] = _frame.data[2];
				pdata.byt[6] = _frame.data[1];
				pdata.byt[7] = _frame.data[0];
			}
			else {
				pdata.byt[0] = _frame.data[0];
				pdata.byt[1] = _frame.data[1];
				pdata.byt[2] = _frame.data[2];
				pdata.byt[3] = _frame.data[3];
				pdata.byt[4] = _frame.data[4];
				pdata.byt[5] = _frame.data[5];
				pdata.byt[6] = _frame.data[6];
				pdata.byt[7] = _frame.data[7];
			}




			switch (listen_can_messages[i].type)
			{
			case CAN_DATA_TYPES::BOOL_1:std::cout << pdata.bool_1 << std::endl; break;
			case CAN_DATA_TYPES::FLOAT_3:std::cout << pdata.float_3 << std::endl; break;
			case CAN_DATA_TYPES::INT_2:std::cout << pdata.int_2 << std::endl; break;
			case CAN_DATA_TYPES::LONG_4:std::cout << pdata.long_4 << std::endl; break;
			default:
				//TODO ADD SIMPLELOG MESSAGE
				break;
			}
			//TODO CONVERT
			int fif = 0;
		}
	}


}


int can_open_port(const char *port)
{
	struct ifreq ifr;
	struct sockaddr_can addr;
	/* open socket */
	soc = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (soc < 0)
	{
		return (-1);
	}
	addr.can_family = AF_CAN;
	strcpy(ifr.ifr_name, port);
	if (ioctl(soc, SIOCGIFINDEX, &ifr) < 0)
	{
		return (-1);
	}
	addr.can_ifindex = ifr.ifr_ifindex;
	fcntl(soc, F_SETFL, O_NONBLOCK);
	if (bind(soc, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		return (-1);
	}
	return 0;
}
int can_send_port(struct can_frame *frame)
{
	int retval;
	retval = write(soc, frame, sizeof(struct can_frame));
	if (retval != sizeof(struct can_frame))
	{
		return (-1);
	}
	else
	{
		return (0);
	}
}
/* this is just an example, run in a thread */
void can_read_port()
{
	struct can_frame frame_rd;
	int recvbytes = 0;
	read_can_port = 1;
	//while (read_can_port)
	//	{
	struct timeval timeout = { 1, 0 };
	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(soc, &readSet);
	if (select((soc + 1), &readSet, NULL, NULL, &timeout) >= 0)
	{
		if (!read_can_port)
		{
			//			break;
		}
		if (FD_ISSET(soc, &readSet))
		{
			recvbytes = read(soc, &frame_rd, sizeof(struct can_frame));
			if (recvbytes)
			{
				//	printf("dlc = %d, data = %s\n", frame_rd.can_dlc, frame_rd.data);
				process_can_frame(frame_rd);
			}
		}
		//}
	}
}
int can_close_port()
{
	close(soc);
	return 0;
}


//SIGNAL HANDLING
void signalHandler(int signum) {
	simple_logger::log("GOT INTERRUPT SIGNAL", simple_logger::LOG_TYPE::INFO);
	simple_logger::stop_log();



	can_close_port();
	LS.Close();
	std::cout << "Interrupt signal (" << signum << ") received.\n";
	exit(signum);
}


int main(int argc, char *argv[])
{
	// REGISTER SIGNAL HANDLER
	signal(SIGINT, signalHandler);

	//STARTUP LOGGER
	if (!simple_logger::start_logger("./log.html")) {
		std::cout << "ERROR - CANT OPEN LOG FILE" << std::endl;
		return -1;
	}
	simple_logger::log("AquaCPU_SMS_SERVICE_STARTED", simple_logger::LOG_TYPE::INFO);


	//CREATE CONF PARSER
	FRM::ini_parser* conf_parser = new FRM::ini_parser();
	conf_parser->load_ini_file("./sms_gateway_conf.ini");

	//	wiringPiSetup();

	//LOAD MESSAGE LIST CSV
	std::string conf_csv = *conf_parser->get_value("path", "listen_gateay_list_csv");
	if (conf_csv == "") {
		conf_csv == "./sms_gateway_messages.csv";
		simple_logger::log("INI_PARSER_FAIL_CONF_CI_FAIL_USING_DEFAULT_INTERFACE_CAN0", simple_logger::LOG_TYPE::WARNING);
	}
	//PARSE THE CAN MESSAGES CSV FILE TO STRUCT
	listen_can_messages.clear();

	std::ifstream input(conf_csv.c_str());
	if (input.fail()) {
		simple_logger::log("SMS_MESSAGE_FILE_CANT_BE_OPEN");
		simple_logger::stop_log();
		std::cout << "ERROR - PLEASE LOOK AT LOGFILE" << std::endl;
		return -1;
	}
	//PARSE THE CSV
	for (std::string line; getline(input, line);)
	{
		//ignore comments ans empty lines like "# "
		if (line.size() == 0 || line == "\n " || (line.at(0) == '#' && line.at(1) == ' '))
		{
			continue;
		}
		//remove windows \r CR
		RemoveChar(line, '\r');
		//count the , chars, becuase each valid line must contain 4 , for 5 collums
		if (count_chars_in_string(line, ',') < 4) {
			simple_logger::log("MESSAGE_FILE_LINE_NOT_COMPLETE (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		//NOW PARSE INFOS
		CAN_MESSAGES_STRUCT tmp_cms;

		//START WITH THE EXTENTION TYPE ID
		const char* start_str = line.c_str();
		if (start_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		const char* end_str = strstr(start_str, ",");
		if (end_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		std::string tmp_string = "";
		tmp_string.append(start_str, end_str);
		if (!check_for_only_numbers(tmp_string)) {
			simple_logger::log("NUM_CECKFAILED_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		tmp_cms.extention_id = atol(tmp_string.c_str());

		//NEXT THE LISTEN CAN ID
		start_str = end_str + 1;
		if (start_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		//start_str = strstr(line.c_str(), ",");
		if (start_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}

		end_str = strstr(start_str, ",");
		if (end_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		tmp_string = "";
		tmp_string.append(start_str, end_str);
		if (!check_for_only_numbers(tmp_string)) {
			simple_logger::log("NUM_CECKFAILED_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		tmp_cms.listen_can_id = atol(tmp_string.c_str());

		//NEXT THE TYPE
		start_str = end_str + 1;
		if (start_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		//		start_str = strstr(line.c_str(), ",");
		if (start_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}

		end_str = strstr(start_str, ",");
		if (end_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		tmp_string = "";
		tmp_string.append(start_str, end_str);
		if (!check_for_only_numbers(tmp_string)) {
			simple_logger::log("NUM_CECKFAILED_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		tmp_cms.type = (CAN_DATA_TYPES)atoi(tmp_string.c_str());

		//NEXT THE BYTE ORDER
		start_str = end_str + 1;
		if (start_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		//		start_str = strstr(line.c_str(), ",");
		if (start_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}

		end_str = strstr(start_str, ",");
		if (end_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		tmp_string = "";
		tmp_string.append(start_str, end_str);
		if (!check_for_only_numbers(tmp_string)) {
			simple_logger::log("NUM_CECKFAILED_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		tmp_cms.byte_order = (ENDIANNESS)atoi(tmp_string.c_str());

		//NEXT THE BYTE ORDER
		start_str = end_str + 1;
		if (start_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		start_str = strstr(line.c_str(), ",\"");
		if (start_str == nullptr) {
			start_str = strstr(line.c_str(), "\"");
			start_str += 1;
			simple_logger::log("INCORRECT_SYNTAX_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
		}
		else {
			start_str += 2;
		}

		if (start_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}

		end_str = strstr(start_str, "\"");
		if (end_str == nullptr) {
			simple_logger::log("STR_PARSING_OF_MESSAGE_FILE_LINE_FAILED (" + line + ")", simple_logger::LOG_TYPE::WARNING);
			continue;
		}
		tmp_string = "";
		tmp_string.append(start_str, end_str);
		tmp_cms.message = tmp_string;

		listen_can_messages.push_back(tmp_cms);

	}
	input.close();
	std::cout << "FILTER LOADED :" << listen_can_messages.size() << std::endl;



	//SETUP CAN0SOCKET
	std::string conf_ci = *conf_parser->get_value("interface", "can_interface");
	if (conf_ci == "") {
		conf_ci == "can0";
		simple_logger::log("INI_PARSER_FAIL_CONF_CI_FAIL_USING_DEFAULT_INTERFACE_CAN0", simple_logger::LOG_TYPE::WARNING);
	}
	//TRY TO OPEN CAN
	if (can_open_port(conf_ci.c_str()) == -1) {
		simple_logger::log("STARTUP_OF_THE_CANPORT_FAILED_INTERFACE_EXISTS", simple_logger::LOG_TYPE::ERROR);
		return -1;
	};


	//OPEN SERIAL PORT



	Ret = LS.Open("/dev/ttyUSB0", 115200);
	if (Ret != 1) {
		simple_logger::log("SERIAL_PORT_OPEN_FAILED", simple_logger::LOG_TYPE::ERROR);
		return -1;
	}

	//TRIGGER RESET PORT




	//sms_messages.push_back("1. TEST");
	//sms_messages.push_back("2. TEST");
	//sms_messages.push_back("3. TEST");
	//sms_messages.push_back("4. TEST");


	std::vector<UART_MESSAGES> uart_msg_queue;
	int send_index = 0;
	//TODO NAKE A SEPERATE SETUP FUNC FOR GSM WITH ERROR MESSAGE
	//TODO READ PIN FROM CONF FILE
	//TODO USE WIRING PI FOR RESET AND SWITCH FOR EN SENING + LED
	//WAIT AT SETUP FOR RDY
	UART_MESSAGES tmp;
	tmp.to_send = "AT";
	tmp.expected = "OK";
	tmp.send = false;
	tmp.noCR = false;
	uart_msg_queue.push_back(tmp);
	tmp.to_send = "AT+CPIN=\"0689\"";
	tmp.expected = "SMS Ready";
	tmp.send = false;
	tmp.noCR = false;
	uart_msg_queue.push_back(tmp);
	tmp.to_send = "AT+CMGF=1";
	tmp.expected = "OK";
	tmp.send = false;
	tmp.noCR = false;
	uart_msg_queue.push_back(tmp);
//	tmp.to_send = "AT+CSMP=17,167,0,16"; //TODO SET THIS AS OPTION
	tmp.to_send = "AT";
	tmp.expected = "OK";
	tmp.send = false;
	tmp.noCR = false;
	uart_msg_queue.push_back(tmp);
	tmp.to_send = "AT+CMGS=\"01714927699\"";
	tmp.expected = "> ";
	tmp.send = false;
	tmp.noCR = true;
	uart_msg_queue.push_back(tmp);
	tmp.to_send = "SMS_GATEWAY:STARTED\x1A";
	tmp.expected = "OK";
	tmp.send = false;
	uart_msg_queue.push_back(tmp);


	//LOAD FIRST MESSAGE
	if (sms_messages.size() > 0) {
		uart_msg_queue.at(5).to_send = sms_messages.back() + "\x1A";
		uart_msg_queue.at(5).send = false;
		//sms_messages.pop_back();
	}

	while (true)
	{
		can_read_port();
	
		if (sms_messages.size() > 0 && send_index != 4) {
			uart_msg_queue.at(5).to_send = sms_messages.back() + "\x1A";
			uart_msg_queue.at(5).send = false;
			sms_messages.pop_back();
			send_index = 4;
			for (size_t i = send_index; i < uart_msg_queue.size(); i++)
			{
				uart_msg_queue.at(i).send = false;
			}
		}




		if (!uart_msg_queue.at(send_index).send) {

			if (send_index == 4 && sms_messages.size() <= 0) {
				//	continue;
			}

			std::cout << "SEND:" << uart_msg_queue.at(send_index).to_send;
			LS.WriteString((uart_msg_queue.at(send_index).to_send + "\r\n").c_str());
			uart_msg_queue.at(send_index).send = true;
		}
		Ret = LS.ReadString(Buffer, '\r\n', 128, 50UL);                                // Read a maximum of 128 characters with a timeout of 5 seconds
		if (Ret > 0) {
			if (std::string(Buffer) == (uart_msg_queue.at(send_index).expected + "\r\n")) {
				//if (send_index == 5) { sms_messages.pop_back(); }
				std::cout << "epxetec message rec:" << Buffer << std::endl;
				if (send_index < uart_msg_queue.size() - 1) {
					send_index++;
					std::cout << "inc" << send_index << std::endl;
				}
				else {
					//LOAD NEW MESSGAE
					if (sms_messages.size() > 0) {
						uart_msg_queue.at(5).to_send = sms_messages.back() + "\x1A";
						uart_msg_queue.at(5).send = false;
						//sms_messages.pop_back();
						send_index = 4;
						for (size_t i = send_index; i < uart_msg_queue.size(); i++)
						{
							uart_msg_queue.at(i).send = false;
						}
					}
				}
			}
			else {
				std::cout << " message rec:" << Buffer << std::endl;
			}
		}
		else
		{
			//IF WE DONT HAVE A CR LIKE "> " we have in the struct a flag nad recieve only the needed bytes
			if (uart_msg_queue.at(send_index).noCR) {

				if (std::string(Buffer) == uart_msg_queue.at(send_index).expected) {
					//	if (send_index == 5) { sms_messages.pop_back(); }
					if (send_index < uart_msg_queue.size() - 1) {
						send_index++;
						std::cout << "inc" << send_index << std::endl;
						LS.WriteString((uart_msg_queue.at(send_index).to_send + "\r\n").c_str());
						uart_msg_queue.at(send_index).send = true;
					}
					else {
						//LOAD NEW MESSGAE
						if (sms_messages.size() > 0) {
							uart_msg_queue.at(5).to_send = sms_messages.back() + "\x1A";
							uart_msg_queue.at(5).send = false;
							//	sms_messages.pop_back();
							send_index = 4;
							for (size_t i = send_index; i < uart_msg_queue.size(); i++)
							{
								uart_msg_queue.at(i).send = false;
							}
						}
					}
				}
				Ret = LS.Read(Buffer, uart_msg_queue.at(send_index).expected.size() + 1, 50UL);
				if (Ret > 0) {
					std::cout << "SINGLE message rec:" << Buffer << std::endl;
				}
			}
		}

	};
	can_close_port();
	LS.Close();




	int bla = 0;

	return 0;
}