#define N 43

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <math.h> 
#include <time.h>
#include <curl/curl.h>
#include <sys/types.h>
#include <regex.h>
#include "bot.h"
#include "curl.h"
#include "op.h"

// Magic function to throw error to stderr or other file
void c_error(FILE *out, const char *fmt, ...){
	// With a va_list we can print string like printf argument
	// es -  "%s", string -
	va_list ap;
	va_start(ap, fmt);
	vfprintf(out, fmt, ap);
	va_end(ap);
}

// Initialize the bot in the struct
void bot_init(struct IRC *bot, char *s, char *p, char *n, char *c){
	strcpy(bot->server, s);
	strcpy(bot->port, p);
	strcpy(bot->nick, n);
	strcpy(bot->chan, c);
	bot->op = (char*)malloc(1);
	bot->opn = 0;
}

// Send message over the connection <3
void bot_raw(struct IRC *bot, char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(bot->sbuf, 512, fmt, ap);
	va_end(ap);
	printf("<< %s", bot->sbuf);
	write(bot->conn, bot->sbuf, strlen(bot->sbuf));
}

// Connect the bot on the network
int bot_connect(struct IRC *bot){
	char *user, *command, *where, *message, *sep, *target;
	int i, j, l, sl, o = -1, start, wordcount;
	char buf[513];

	// So many socket things :(	
	memset(&(bot->hints), 0, sizeof bot->hints);
	bot->hints.ai_family = AF_INET;
	bot->hints.ai_socktype = SOCK_STREAM;
	getaddrinfo(bot->server, bot->port, &(bot->hints), &(bot->res));
	bot->conn = socket(bot->res->ai_family, bot->res->ai_socktype, bot->res->ai_protocol);
	
	// Try to connect
	if(connect(bot->conn, bot->res->ai_addr, bot->res->ai_addrlen)<0){
		c_error(stderr,"Error: Connection Failed\n");
		return -1;
	}
	
	// From RFC: USER <username> <hostname> <servername> <realname>
	bot_raw(bot,"USER R2-D2 hosts fn R2-D2\r\n");
	// From RFC: NICK <nickname>
  bot_raw(bot,"NICK %s\r\n", bot->nick);
	
	// All the program remain here waiting for channel-input
	while ((sl = read(bot->conn, bot->sbuf, 512))) {
		// Read the message char by char
		for (i = 0; i < sl; i++) {
			o++;
			buf[o] = bot->sbuf[i];
			// If the char is  endline \r\n
			if ((i > 0 && bot->sbuf[i] == '\n' && bot->sbuf[i - 1] == '\r') || o == 512) {
				buf[o + 1] = '\0';
				l = o;
				o = -1;
				
				// Log into the terminal
				printf(">> %s", buf);
				
				// When there is a PING, reply with PONG
				if (!strncmp(buf, "PING", 4)) {
					buf[1] = 'O';
					bot_raw(bot,buf);
				} else if (buf[0] == ':') {
					wordcount = 0;
					user = command = where = message = NULL;
					for (j = 1; j < l; j++) {
						if (buf[j] == ' ') {
							buf[j] = '\0';
							wordcount++;
							switch(wordcount) {
								case 1: user = buf + 1; break;
								case 2: command = buf + start; break;
								case 3: where = buf + start; break;
							}
							if (j == l - 1) continue;
							start = j + 1;
						} else if (buf[j] == '=' && wordcount == 3) {
							wordcount--;
						} else if (buf[j] == '#' && wordcount == 3) {
							wordcount--;
							if (j < l - 1) target = buf + j;
						} else if (buf[j] == ':' && wordcount == 3) {
							if (j < l - 1) message = buf + j + 1;
							break;
						} else if (wordcount == 3)  {
							if (j < l - 1) message = buf + j;
							break;
						}
					}
			
					if (wordcount < 2) continue;
					
					if (!strncmp(command, "001", 3) && bot->chan != NULL) {
						// From RFC: JOIN <channel> [key]  
						// Maybe we can support password protected channel (?)
						bot_raw(bot,"JOIN %s\r\n", bot->chan);
						bot_raw(bot,"PRIVMSG %s :Ciao a tutti da %s!\r\n", bot->chan, bot->nick);
					} else if (!strncmp(command, "PRIVMSG", 7) || !strncmp(command, "NOTICE", 6)) {
						if (where == NULL || message == NULL) continue;
						if ((sep = strchr(user, '!')) != NULL) user[sep - user] = '\0';
						if (where[0] == '#' || where[0] == '&' || where[0] == '+' || where[0] == '!') target = where; else target = user;
						//TODO: Handle the function Return
						bot_parse_action(bot, user, command, where, target, message);
					} else {
						//clean the message
						message = strtok(message, "\n\r");
						bot_parse_service(bot, user, command, where, target, message);
					}
					/* Rejoin after kick */
					if((strcasecmp(command, "KICK") == 0) && (REJOIN) ){
						sleep(3);
						bot_raw(bot,"JOIN %s\n\r", bot->chan);
						continue;
					}
					/* Saluta i nuovi arrivati */
					if((strcasecmp(command, "JOIN") == 0) && (HELLO) ){
						/* implementare */
					} 




				}
			}
		} 
	}
	return 0;
}

int bot_parse_action(struct IRC *bot, char *user, char *command, char *where, char *target, char *msg){
// Private message example
//	[from: Th3Zer0] [reply-with: PRIVMSG] [where: C-3PO_bot] [reply-to: Th3Zer0] ciao
// Channel message example
//	[from: Th3Zer0] [reply-with: PRIVMSG] [where: ##freedomfighter] [reply-to: ##freedomfighter] ciao
	


	if(DEBUG){
		printf("[from: %s] [reply-with: %s] [where: %s] [reply-to: %s] \"%s\"", user, command, where, target, msg);
	}
	
	
	//comandi senza "!"
	if(strstr(msg,"<3") || strstr(msg,"love"))
		bot_raw(bot,"PRIVMSG %s :%s: so much LOVE\r\n", target, user);
	else if(strstr(msg,"lol"))
		bot_raw(bot,"PRIVMSG %s :%s: i'm very happy ( ͡° ͜ʖ ͡°) \r\n", target, user);
	else if(strstr(msg,"\\o/"))
		bot_raw(bot,"PRIVMSG %s :%s: \\o/ \r\n", target, user);
	 
   
    struct hostent* hp;
    struct in_addr **addr_list;
	
	char *h1=malloc(strlen(bot->nick)+14);
	strcpy(h1,bot->nick);
	strcat(h1,": come stai?");
	if(strstr(msg,h1)) {
    		bot_raw(bot, "PRIVMSG %s :%s: non male grazie, tu?\r\n", target, user);
  	}
	free(h1);
	h1=NULL;
  
	if(strcasecmp(user,"NickServ")==0){
		if(strstr(msg,"Last seen")){
			bot_raw(bot, "PRIVMSG %s :%s\r\n", target, msg);
		}
	}
	
		
	if(strstr(msg,"http://")) {
		msg = strtok(msg, "\n\r");
		if(is_html_link(msg)){
			char *title=malloc(1);
			if(html_title(msg,title)==0){
				bot_raw(bot, "PRIVMSG %s :%s\r\n", target, title);
			}
		}
  	}
	if((msg[0] == '#') && (msg[1]=='!')) {
		bot_raw(bot, "PRIVMSG %s :Shebang!\r\n", target);
	}
	
	if(*msg != '!')
		return 1;

	char *cmd;
	//char *arg[6];
	int i=0;
	char **ap, *argv[10];
	
	

	

		
	// Clean the string
	cmd = strtok(msg, "!");	
	cmd = strtok(cmd, "\n\r");
	// Split the argument
	for(ap = argv; (*ap = strsep(&cmd, " \t")) != NULL;){
		if(**ap != '\0'){
			if(++ap >= &argv[10]){
				break;
			}
		}
	}
	// Count the argument
	while(argv[i] != NULL){
		i++;
	}
	// the command is in the first argument
	if(argv[0] == NULL)
		return 1;	
	
	srand(time(NULL));
	
 char stringa[N][12]={"<3","love","fuck","lastseen","ping","help","portale",
     "count","quit","unaway","away","google","ddg","reddit","sqrt","sum",
     "sub","div","mul","pow","source","eq","archwiki","whoami","attack",
     "lookup","life","rms","random","future","sexy","dio","blasfem","information","grazie",
     "C","contrib","wtf","hate","segfault","yt","userlist","lol"};
      
 int scelta,control_cmd=0;
 int contaarg=0;
 char *stringa_cat ;  //si potrebbe usare anche alloc

	for(scelta=0;scelta<N;scelta++){
		
		if(strcmp(stringa[scelta],argv[0]) == 0)
			break;
	 
	}
	    
	  
    
    /*Tips per aggiungere comandi: 
     * 1) Modificare la costante N in cima 
     * 2) Aggiungere il comando nella matrice "stringa" 
     * 3) Aggiungere case Switch
     * 
     * Ps: E' stato aggiunto un controllo sui  comandi <3 , love , lol perchè vengono eseguiti senza "!"
     * (Vedi sopra , riga 173) , per ogni comando senza "!" va aggiunta un eccezione nella matrice "stringa" 
     * Dunque inserire solo comandi che iniziano con "!" es "!future".
     */
    
    
    switch(scelta){
		
		
		case 0: //<3
		
			control_cmd=1; 
			
			break; 
			
	    	case 1: //love
	    
			control_cmd=1;
			
			break; 
			
	 	case 2: //fuck
	    
			bot_raw(bot,"PRIVMSG %s :%s: don't say bad words!\r\n", target, user);break; 
	    
	    	case 3: //lastseen
	    
			if(argv[1] != NULL) 
				bot_raw(bot, "PRIVMSG NickServ :info %s\r\n",argv[1]); break;
			
	    	case 4: //ping
	    
			bot_raw(bot,"PRIVMSG %s :pong\r\n", target); break;
	    
		case 5: //help
	    
			bot_help(bot,argv[1], target); break;
			
	        case 6: //portale
	    
			bot_portal(bot,argv[1], target); break;
			
	        case 7: //count
	    
			bot_raw(bot,"PRIVMSG %s :%d\r\n", target, i); break; 
			
	        case 8: //Quit
	    
			if(is_op(bot,user)!=-1){
					bot_quit(bot);
				}else if (argv[1] != NULL){
					bot_raw(bot,"PRIVMSG %s :Pff, %s go away!\r\n", target, argv[1]);   
			    }else{ 
					bot_raw(bot,"PRIVMSG %s :Pff, %s go away!\r\n", target, user);
				} break; 
		    
		case 9: //unaway
		
			if(is_op(bot,user)!=-1){
				bot_raw(bot,"AWAY :\0\r\n", target, user);
				//bot_raw(bot,"AWAY\r\n");
			}else{
				bot_raw(bot,"PRIVMSG %s :Pff, %s go away!\r\n", target, user);  
			}break;	
		
		case 10: //away
		
			if(is_op(bot,user)!=-1){
				bot_away(bot);
			}else if(argv[1] != NULL){
				bot_raw(bot,"PRIVMSG %s :Pff, %s go away!\r\n", target, argv[1]);  
			}else{			
				bot_raw(bot,"PRIVMSG %s :Pff, %s go away!\r\n", target, user);
			}break;	
			
		case 11: //google
		
			if(argv[1] != NULL){
				if(argv[2] != NULL)
					bot_raw(bot,"PRIVMSG %s :%s: http://lmgtfy.com/?q=%s\r\n", target, argv[2], argv[1]);  
				else 
					bot_raw(bot,"PRIVMSG %s :%s: http://lmgtfy.com/?q=%s\r\n", target, user, argv[1]);
				}break;		
			
			
		case 12: //ddg
		
			if(argv[2] != NULL)
				bot_raw(bot,"PRIVMSG %s :%s: https://lmddgtfy.net/?q=%s\r\n", target, argv[2], argv[1]); 
			else 
				bot_raw(bot,"PRIVMSG %s :%s: https://lmddgtfy.net/?q=%s\r\n", target, user, argv[1]);break;		
			
		
		case 13: //reddit
		
			if(argv[1] != NULL) 
				bot_raw(bot, "PRIVMSG %s :http://www.reddit.com/search?q=%s\r\n", target, argv[1]);
			else
				bot_raw(bot, "PRIVMSG %s :%s: http://www.reddit.com/r/random\r\n", target, user);break;	
			
		case 14: //sqrt
			
			if(argv[1]!=NULL){
				double i = atof(argv[1]);
				bot_raw(bot,"PRIVMSG %s :%s: %g\r\n", target, user, sqrt(i));  
			} else 
				bot_raw(bot,"PRIVMSG %s :%s: you need to provide at least one argument!\r\n",target,user);break;	
			
			
		case 15: //sum
		
			if((argv[1] != NULL)&&(argv[2] != NULL)) {
				double i = atof(argv[1]);
				double j = atof(argv[2]);
				bot_raw(bot, "PRIVMSG %s :%s: %g\r\n", target, user, i+j); 
			} else 
				bot_raw(bot, "PRIVMSG %s :%s: you need to provide at least two arguments!\r\n", target, user);break;	
			
		/* Aggiunta da CavalloBlu  */ 	
		
		case 16: //sub
		
			if((argv[1] != NULL)&&(argv[2] != NULL)) {
				double i = atof(argv[1]);
				double j = atof(argv[2]);
				bot_raw(bot, "PRIVMSG %s :%s: %g\r\n", target, user, i-j);  
			} else {
				bot_raw(bot, "PRIVMSG %s :%s: you need to provide at least two arguments!\r\n", target, user);
			}break;	
		
		case 17: //div
				
			if((argv[1] != NULL)&&(argv[2] != NULL)){
				double i=atof(argv[1]);
				double j=atof(argv[2]);
				bot_raw(bot, "PRIVMSG %s :%s: %g\r\n", target, user, i/j);  
			} else {
				bot_raw(bot, "PRIVMSG %s :%s: you need to provide at least two arguments!\r\n", target, user);
			}break;			
			
		case 18: //mul
		
			if((argv[1] != NULL)&&(argv[2] != NULL)){
				double i=atof(argv[1]);
				double j=atof(argv[2]);
				bot_raw(bot, "PRIVMSG %s :%s: %g\r\n", target, user, i*j); 
			} else {
				bot_raw(bot, "PRIVMSG %s :%s: you need to provide at least two arguments!\r\n", target, user);
			}break;	
			
		case 19: //pow
		
			if((argv[1] != NULL)&&(argv[2] != NULL)){
				double i=atof(argv[1]);
				double j=atof(argv[2]);
				bot_raw(bot, "PRIVMSG %s :%s: %g\r\n", target, user, pow(i,j));  
			} else {
				bot_raw(bot, "PRIVMSG %s :%s: you need to provide at least two arguments!\r\n", target, user);
			}break;	
			
		case 20: //source
			
			if(argv[1]!=NULL)
				bot_raw(bot, "PRIVMSG %s :%s: https://github.com/umby213/bot\r\n", target, argv[1]); 
			else
				bot_raw(bot, "PRIVMSG %s :%s: https://github.com/umby213/bot\r\n", target, user);break;	
				
		case 21: //eq
		
			if(argv[1]!=NULL&&argv[2]!=NULL&&argv[3]!=NULL){
				
				float a,b,c;
				float x1,x2,delta;
				
				a=atof(argv[1]);
				b=atof(argv[2]);
				c=atof(argv[3]);
				delta=b*b - 4*a*c;
				
				if(a==0){
					
					if(b!=0){
						
						x1=-c/b;
						x2=0;
						bot_raw(bot, "PRIVMSG %s :%s: x1: %.2f x2: %.2f\r\n", target, user,x1,x2);
					}
					else{
						bot_raw(bot, "PRIVMSG %s :%s: L'equazione non contiene variabili.\r\n", target, user);
					}
				}
				else{
					
					if(delta>=0){
						
						x1=(-b+sqrt(delta))/(2*a);
						x2=(-b-sqrt(delta))/(2*a);
						bot_raw(bot, "PRIVMSG %s :%s: x1: %.2f x2: %.2f\r\n", target, user,x1,x2);
					}
					else{
						bot_raw(bot, "PRIVMSG %s :%s: L'equazione non ammette soluzioni reali.\r\n", target, user);
					}

				}
			} /* fine if di esistenza argc */
				
				else{
					bot_raw(bot, "PRIVMSG %s :%s: Parametri insufficienti.\r\n", target, user);
				}
				break;	
		/* Fine CavalloBlu  */

		case 22:  //archwiki
		
			if(argv[1] != NULL) 
					
				bot_raw(bot,"PRIVMSG %s ::%s https://wiki.archlinux.org/index.php?title=Special:Search&search=%s\r\n", target, user, argv[1]);
		
			else 
				bot_raw(bot,"PRIVMSG %s :%s: Errore: Inserisci almeno un argomento da cercare nella wiki", target, user);break;	
		
		case 23: //whoami
		
			bot_raw(bot,"PRIVMSG %s :I'm a bot, developed using Coffee. You are %s, I think you know...\r\n", target, user);break;	
			
		case 24: //attack
		
			srand(time(NULL));
			int critical;
			critical = (rand()%10)/8;
			
			if(critical)
			{
				bot_raw(bot,"PRIVMSG %s :\001ACTION attacks %s! %d damage (it's super effective).\001\r\n", target, argv[1], rand()%20 + 21);
			}
			else 
			{
				bot_raw(bot,"PRIVMSG %s :\001ACTION attacks %s! %d damage.\001\r\n", target, argv[1], rand()%20 + 1);
			}
			break;	
				
		case 25: //lookup
		
			//era qui struct
			
			
			
			if((hp = gethostbyname(argv[1])) == NULL)
			{
				bot_raw(bot,"PRIVMSG %s :%s: The host %s is unreachable.\r\n", target, user, argv[1]);
			}
			else {
				
				addr_list = (struct in_addr **)hp->h_addr_list;
				bot_raw(bot,"PRIVMSG %s :%s: IP: %s\r\n", target, user, inet_ntoa(*addr_list[0]));
			}break;	
			
		case 26: //life
		
			bot_raw(bot, "PRIVMSG %s :%s: 42\r\n", target, user);break;	  	
				
		case 27: //rms
		 
			bot_raw(bot, "PRIVMSG %s :Stallman approves.\r\n", target);break;	 	
			 
		case 28: //random
		 
			
			bot_raw(bot, "PRIVMSG %s :%s: here you are a $RANDOM number -> %d\r\n", target, user, rand());break;	 
				 
		case 29: //future
		
			bot_raw(bot, "PRIVMSG %s :Innovation is not what innovators do, but what customers adopt...this is freedomfight!\r\n", target);break;	 
		
		case 30: //sexy
		
			bot_raw(bot, "PRIVMSG %s :https://rms.sexy/\r\n", target); break;	
		
		case 31: //dio
		
			bot_raw(bot, "PRIVMSG %s :ah quel famoso porco di tre lettere...sì, ora ricordo...\r\n", target);break;	 
			
		case 32: //blasfem
		
			bot_raw(bot, "PRIVMSG %s :dioladro.\r\n", target);break;		
			
		case 33: //information
		
			bot_raw(bot, "PRIVMSG %s :Information is power. But like all the power there are those who want to keep it for themselves -- ~Aaron Swartz\r\n", target); break;	
		
		case 34: //grazie
		
			bot_raw(bot, "PRIVMSG %s :prego %s!\r\n", target, user);break;	 
			
		case 35: //C
		
			bot_raw(bot, "PRIVMSG %s :sono stato scritto in C per far contento RMS.\r\n", target);break;	 
		
		case 36: //contrib
		
			if (argv[1] != NULL)
				bot_raw(bot, "PRIVMSG %s :%s: https://raw.githubusercontent.com/umby213/bot/master/contributors.txt\r\n", target, argv[1]);
			else
				bot_raw(bot, "PRIVMSG %s :https://raw.githubusercontent.com/umby213/bot/master/contributors.txt\r\n", target);break;	
				
		case 37: //wtf
		
			if(argv[1] != NULL) 
				bot_raw(bot, "PRIVMSG %s :%s: http://goo.gl/sAkn7z\r\n", target, argv[1]);
			else 
				bot_raw(bot, "PRIVMSG %s :%s: http://goo.gl/sAkn7z\r\n", target, user);break;	
			
		case 38: //hate
		
			if(argv[1] != NULL) 
				bot_raw(bot, "PRIVMSG %s :%s: http://goo.gl/fHVO8s\r\n", target, argv[1]);
			else 
				bot_raw(bot, "PRIVMSG %s :%s: http://goo.gl/fHVO8s\r\n", target, user);break;	
				
		case 39: //segfault
			
			bot_raw(bot, "PRIVMSG %s :TrinciaPollo never segfault...and never lie.\r\n", target);break;	
			
		case 40: //yt
		
			if(argv[1] != NULL) {
				
				char *yt;
				strtok(argv[1], "?");	
				yt = strtok(NULL, "?");
				
			if(yt[0]=='v' && yt[1]=='='){
				yt = strtok(&yt[2], "&");
				bot_raw(bot, "PRIVMSG %s :%s Click Here: http://www.flvto.com/it/download/yt_%s/\r\n", target, user, yt);
			}
			
			} else 
				bot_raw(bot, "PRIVMSG %s :%s Dowload and extract .mp3 from a video: `youtube-dl --extract-audio --audio-format mp3 link_here`\r\n", target, user);break;	
		
		case 41: //userlist
			
			if (strcasecmp(target,bot->chan) != 0)
				bot_raw(bot, "PRIVMSG %s :!userlist non può essere usato in query.\r\n", target);
			else
				bot_raw(bot, "NAMES %s\r\n", target); break;	
				
		case 42: //lol
		
			control_cmd = 1;
		
		
		default: //in ogni altro caso
		
		if (control_cmd != 1)
			bot_raw(bot, "PRIVMSG %s :%s: Comando non valido, dai un !help.\r\n", target, user); 
		break;	
		
				

			
	}		

	
	
	return 0;
	
	
	
}

int bot_parse_service(struct IRC *bot, char *server, char *command, char *me, char *channel, char *msg){
	if(DEBUG){
		printf("[server: %s] [command: %s] [me: %s] [channel: %s] %s\n",server,command,me,channel,msg);
	}
	
	// 353 is the NAMES list
	if(strcasecmp(command, "353") == 0){
		parse_op(bot,msg);
		if(DEBUG){
			print_op(bot);
		}
	}
	if(strcasecmp(command, "MODE") ==0){
		if((msg[0]=='+') && (msg[1]=='o')){
			// if it's not already in the oplist
			if(is_op(bot,&msg[3])==-1){
				add_op(bot,&msg[3]);
				if(DEBUG){
					print_op(bot);
				}
			}
		}
		else if((msg[0]=='-') && (msg[1]=='o')){
			if(is_op(bot,&msg[3])!=-1){
				rm_op(bot,&msg[3]);
				if(DEBUG){
					print_op(bot);
				}
			}
		}
	}
	
	return 0;
}


// bot_join: For joining a channel
void bot_join(struct IRC *bot, const char *chan){
	bot_raw(bot,"JOIN %s\r\n", chan);
}
// bot_part: For leaving a channel
void bot_part(struct IRC *bot, const char *chan){
	bot_raw(bot,"PART %s\r\n", chan);
}
// bot_nick: For changing your nick
void bot_nick(struct IRC *bot, const char *data){
	bot_raw(bot,"NICK %s\r\n", data);
}
// bot_away: For quitting IRC
void bot_away(struct IRC *bot){
	bot_raw(bot,"AWAY :%s Bot\r\n",bot->nick);
}
// bot_quit: For quitting IRC
void bot_quit(struct IRC *bot){
	bot_raw(bot,"QUIT :%s Bot\r\n",bot->nick);
}
// bot_topic: For setting or removing a topic
void bot_topic(struct IRC *bot, const char *channel, const char *data){
	bot_raw(bot,"TOPIC %s :%s\r\n", channel, data);
}
// bot_action: For executing an action (.e.g /me is hungry)
void bot_action(struct IRC *bot, const char *channel, const char *data){
	bot_raw(bot,"PRIVMSG %s :\001ACTION %s\001\r\n", channel, data);
}
// bot_msg: For sending a channel message or a query
void bot_msg(struct IRC *bot, const char *channel, const char *data){
	bot_raw(bot,"PRIVMSG %s :%s\r\n", channel, data);
}
void bot_help(struct IRC *bot, char* cmd, char* target){
	if(cmd==NULL){
		char h1[] = "!help !ping !quit !google !ddg !reddit !archwiki !whoami !attack !lookup !away !life !rms !random !privacy !segfault !future !sum !sub !div !pow !sqrt !eq !C !grazie !source !portale <3";
		bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		char h2[] = "Type !help <cmd> for information about that command.";
		bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h2);
	} else {
		if(strcasecmp(cmd, "ping") == 0){
			char h1[] = "Perform a ping to the bot. Reply with pong";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "quit") == 0){
			char h1[] = "Quit the bot";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "reddit") == 0){
		        char h1[] = "Print the link of a reddit search. Take <keyword> as argument";
		        bot_raw(bot, "PRIVMSG %s :%s\r\n", target, h1);
    		}
		else if(strcasecmp(cmd, "google") == 0){
			char h1[] = "Print the link of a google search. Take <keyword> as argument";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "ddg") == 0){
			char h1[] = "Print the link of a google search. Take <keyword> as argument";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "sqrt") == 0){
			char h1[] = "Perform the square root of a number. Take <number> as argument";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "archwiki") == 0){
			char h1[] = "Print the link of an archwiki page. Take <keyword> as argument";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "whoami") == 0){
			char h1[] = "Whoami?";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "attak") == 0){
			char h1[] = "Let's kick some asses";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "future") == 0){
			char h1[] = "the future we would like to.";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "lookup") == 0){
			char h1[] = "Perform a DNS Lookup. Take <hostname> as argument";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "sub") == 0){
			char h1[] = "Sottrae due numeri: !sub <n> <m>";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "sum") == 0){
			char h1[] = "Somma due numeri: !sum <n> <m>";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "div") == 0){
			char h1[] = "Divide due numeri: !div <n> <m>";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}    
		else if(strcasecmp(cmd, "mul") == 0){
			char h1[] = "Moltiplica due numeri: !mul <n> <m>";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "pow") == 0){
			char h1[] = "Elevamento a potenza: !pow <base> <esponente>";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "away") == 0){
			char h1[] = "Set the bot Away";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "life") == 0){
			char h1[] = "You ask the meaning of life?";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "rms") == 0){
			char h1[] = "Who don't know RMS?";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "random") == 0){
			char h1[] = "Random?";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "privacy") == 0){
			char h1[] = "Some privacy sites";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "segfault") == 0){
			char h1[] = "Pfff. I'm not a liar.";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "source") == 0){
			char h1[] = "Visualizza la pagina contenente il codice di ";
			bot_raw(bot,"PRIVMSG %s :%s%s\r\n", target, h1, bot->nick);
		}
		else if(strcasecmp(cmd, "eq") == 0){
			char h1[] = "Risolve un'equazione di secondo grado. Richiede tre parametri.";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "portale") == 0){
			char h1[] = "Visualizza link alle principali sezioni del portale";
			bot_raw(bot, "PRIVMSG %s :%s\r\n", target, h1);
		}
		else{
			char h1[]="Non esiste alcun comando simile.";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target,h1);
		}
	}
} // void bot_help end

void bot_portal(struct IRC *bot, char* cmd, char *target){
	if(cmd==NULL){
		char h1[] = "didattica, login";
		bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
	/*	char h2[] = "Type !help <cmd> for information about that command.";
		bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h2);*/
	} else {
		if(strcasecmp(cmd, "didattica") == 0){
			char h1[] = "https://didattica.polito.it/";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
		else if(strcasecmp(cmd, "login") == 0){
			char h1[] = "https://login.didattica.polito.it/secure-studenti/ShibLogin.php";
			bot_raw(bot,"PRIVMSG %s :%s\r\n", target, h1);
		}
	} // else of if(cmd==null) end;


} //void bot_portal end
		
