# webserver
Web server made in C!

This program takes one argument, the port that the webserver will listen on. The server uses TCP and is multithreaded to have a maximum of 10 client connections. When connected, the client is sent the HTML of the folder 'web'. 
