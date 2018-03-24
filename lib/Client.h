#ifndef _Client_h
#define _Client_h 1

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "types.h"
#include "Node.h"
#include "Certificate.h"
#include "Certificate_order.h"

class Reply;
class Request;
class ITimer;
class Order_request;
//class Committed_locally;
//class Local_commit;
class Mycommitted_locally;//改
class mycommit;//改

extern void retransmit_request_handler();
extern void start_commit_handler();
//extern void retransmit_commit_handler();
extern void retransmit_reply_handler();

extern void rtimer_handler();
enum state_e {No_out_request_state=0, Sent_request_state=1,Local_commit_state=2,
	      Request_committed_state=3};

 
 // ZYZZYVA : Client state machine : 
 // 1. request-sent (RS) : request is sent and conservative-retransmit-timer is started
 //      - if 2f+1 OR messages are matching: stop crt timer and go to step 2 
 //      - Timeout : retransmit request if fewer than 2f+1 OR (ordered-request) are recived
 //      - Timeout : Trigger commit phase (step 3) if at least 2f+1 OR messages are received but 
 //                  less than 2f+1 messages are matching  
 // 2. request-ordered (RO) : start the aggressive-start-commit timer
 //      - received 3f+1 matching OR messages : stop the asc timer and go to step 4
 //      - Timeout : stop the aggressive-start-commit timer and go to step 3     
 // 3. commit-started (CS) : Start the commit phase by broadcasting LC requests and 
 //                     start the retransmit-commit-request
 //      - received 3f+1 matching OR messages or 2f+1 matching CL (committed-locally) messages :
 //        go to step 4  
 //      - Timeout : retransmit LC (local-commit) request  
 // 4. request-committed (RC): Start reply-timer and wait for f+1 matching responses    
 //      - if f+1 matching reply messages : stop the timer and go to 5.
 //      - Timeout : broadcast retransmit-reply messages to the replicas
 // 5. reply-received(RR) : if request-committed is true and it receives f+1 matching replies
                          
class Client : public Node {
public:
  Client(FILE *config_file, FILE *config_priv, short port=0);
  // Effects: Creates a new Client object using the information in
  // "config_file" and "config_priv". The line of config assigned to
  // this client is the first one with the right host address (if
  // port==0) or the first with the right host address and port equal
  // to "port".

  virtual ~Client();
  // Effects: Deallocates all storage associated with this.

  bool send_request(Request *req);
  // Effects: Sends request m to the service. Returns FALSE iff two
  // consecutive request were made without waiting for a reply between
  // them.

  Reply *recv_reply();
  // Effects: Blocks until it receives enough reply messages for
  // the previous request. returns a pointer to the reply. The caller is
  // responsible for deallocating the request and reply messages.

  Request_id get_rid() const;
  // Effects: Returns the current outstanding request identifier. The request
  // identifier is updated to a new value when the previous message is
  // delivered to the user.

  void reset();
  // Effects: Resets client state to ensure independence of experimental
  // points.

  void reset_const(Reply *rep);
  // Reset the constants

  void retransmit();
  // Retransmit request 
  
   bool check_replies();
  //bool send_local_commit();//改 注释掉
  // Sends local commit message

  // Timeout handlers
  ITimer *retransmit_request_timer; // Conservative retransmit request timer
  friend void retransmit_request_handler();
  
  ITimer *start_commit_timer;   // Aggressive start commit timer 
  friend void start_commit_handler();

 /* ITimer *retransmit_commit_timer; // Conservative retransmit commit timer
  friend void retransmit_commit_handler();*///改 不用要这个重传

  ITimer *retransmit_reply_timer; // Conservative retransmit reply timer
  friend void retransmit_reply_handler();
 
  int a_timeout() const; // Return the aggressive timeout
  int c_timeout() const; // Return the conservative timeout
  void check_commit_opt(); // Check commit optimization counter

  Cycle_counter ctimer;
   
private:
  Request *out_req;     // Outstanding request
  bool need_auth;       // Whether to compute new authenticator for out_req
  Request_id out_rid;   // Identifier of the outstanding request
  int n_retrans;        // Number of retransmissions of out_req
  int ctimeout;         // Conservative Timeout period in msecs
  int rtimeout;         // Retransmission Timeout period in msecs
  int atimeout;         // Aggressive timeout period
  bool start_local_commit; // Flag to start local commit aggressively
  bool creset;

  // Maximum retransmission timeout in msecs
  static const int Max_rtimeout = 1000;

  // Minimum retransmission timeout after retransmission 
  // in msecs
  static const int Min_rtimeout = 10;

  Cycle_counter latency; // Used to measure latency.

  // Multiplier used to obtain retransmission timeout from avg_latency
  static const int Rtimeout_mult = 4; 

  // Digest
  Order_request *primary_req;

  // Local-commit message cache
  //Local_commit *lc_cache;//改 这里不需要
  //bool lc_complete;//改 这里不需要根据reply的数目构造local_commit消息发送

  // Read only optimization 
  bool read_only_opt;
  
  // Commit opt flag
  bool commit_opt_flag;
 
  Certificate<Mycommitted_locally> *mycl_reps; // Certificate with mycommitted-locally messages 
  //看是否收到2f+1个Mycommitted_locally消息
  
  //Certificate<Committed_locally> *c_reps; // Certificate with committed-locally messages
  Certificate_order<Reply> *r_reps; // Certificate with application replies

  void retransmit(Request *req);
  // Effects: Retransmits request, local-commit, reply-retransmit, and last new-key messages


  void send_new_key();
  // Effects: Calls Node's send_new_key, and cleans up stale replies in
  // certificates.
  
  Reply *app_reply();
  // Return application reply if reps certificate is complete
};

inline Request_id Client::get_rid() const { return out_rid; } 
inline int Client::c_timeout() const {return ctimeout;}
inline int Client::a_timeout() const {return atimeout;}


#endif // _Client_h

