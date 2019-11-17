Mobile Sensory Network Relay

The Mobile Sensory Network Relay is a sensor application with the following attributes:
	- Runs over TCP topology
	- Contains the following entities:
		-- Sensors: clients that exist at a specific (x,y) coordinate location on the grid
		-- Base Stations: stations that exist at a specific (x,y) coordinate location on 	the grid
		-- Control: server that acts as an intermediary between sensor-to-sensor 		   communication or sensor-to-base-station communication

For our application, we have created a server file and a client file, with the server file containing the CONTROL server and BASE STATION functionalities, and the client containing the sensor functionalities. Sensors are not able to directly communicate with other sensors, so they have to send messages to the CONTROL server (which also control the base stations) in order to traverse through the grid be within range to communicate with the sensor/base station.

For both the client and the server, there were to interfaces. The first interface listens to commands from the user, such as SENDDATA or QUIT, that carries out a certain functionality for intercommunication processing or for self-processing. The second interface waits on the server or the client to send over information, interpret it, and provide an adequate response. For this dual-functionality, we decided to use threads. The child thread will listen in on the commands, while the parent thread listens to user commands. This way, when the parent thread decides to terminate the application, the child thread would also terminate.