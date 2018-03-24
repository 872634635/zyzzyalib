//已改
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <unistd.h>
#include <sys/time.h>

#include "th_assert.h"
#include "Client.h"
#include "ITimer.h"
#include "Message.h"
#include "Reply.h"
#include "Request.h"
#include "Order_request.h"
//#include "Local_commit.h"
//#include "Committed_locally.h"
#include "Mycommit.h"//改
#include "Mycommitted_locally.h"//改
#include "Certificate_order.h"
#include "Certificate.h"

// Force template instantiation
/*#include "Certificate.t"
template class Certificate<Committed_locally>;*///改 去掉 

#include "Certificate.t"
template class Certificate<Mycommitted_locally>;//改

#include "Certificate_order.t"
template class Certificate_order<Reply>;


//#define ADJUST_RTIMEOUT 1
// Current state of the client
enum state_e cur_state; // 1: SR 2: RO 3: CS 4: RC 5: RR  check client state machine

Client::Client(FILE *config_file, FILE *config_priv, short port) : Node(config_file, config_priv, port)  {

  // Fail if node is a replica.
  if (is_replica(id())) th_fail("Node is a replica");
  ctimeout = 150; // Conservative timeout value
  atimeout = 10;  // Aggressive timeout
  commit_opt_flag = false;

  // Timers 
  // WARNING : FIX TIMER BELOW TO SMALL VALUE
  retransmit_request_timer = new ITimer(ctimeout, retransmit_request_handler);
  start_commit_timer = new ITimer(atimeout, start_commit_handler);
  //注释掉 retransmit_commit_timer = new ITimer(ctimeout, retransmit_commit_handler);
  retransmit_reply_timer = new ITimer(2*ctimeout, retransmit_reply_handler);

  out_rid = new_rid();
  out_req = 0;     
  
  if (prot_5f == 1) {
    // If using 5f+1 nodes 
   // c_reps = new Certificate<Committed_locally>(4*f()+1,4*f()+1,5*f()+1);
    // We just need 4f for correct as primary sends OR instead of speculative reply (primay opt) 
    r_reps = new Certificate_order<Reply>(4*f()+1,4*f()+1,5*f()+1); 
	mycl_reps = new Certificate<Mycommitted_locally>(4*f()+1,4*f()+1,5*f()+1);//改
  }
  else {
    // If using 3f+1 nodes 
    //改 注释掉c_reps = new Certificate<Committed_locally>(2*f()+1,2*f()+1,3*f()+1);
	mycl_reps = new Certificate<Mycommitted_locally>(2*f()+1,2*f()+1,3*f()+1);//改
    r_reps = new Certificate_order<Reply>(2*f()+1,2*f()+1,3*f()+1);
  }

  // Initialize
  cur_state = No_out_request_state;
  //lc_cache = 0;改暂时注释掉
  //lc_complete = false;//什么local_commit完成 收到2f+1个mycommitted_locally消息
  start_local_commit = false;

  // Multicast new key to all replicas.
  send_new_key();
  atimer->start(); // authentication freshness timers
}

Client::~Client() {
  delete retransmit_request_timer;
  delete start_commit_timer;
  //改 注释掉 delete retransmit_commit_timer;
  delete retransmit_reply_timer;
}

void Client::reset() {
  ctimeout = 150;
  atimeout = 10;
}

// return application reply if there are enough correct responses
Reply* Client::app_reply() {
  Reply *rep;
  return rep = (r_reps->cvalue()->full()) ?  r_reps->cvalue() : 0;
}

// reset local constants/timers after successfully executing a request
void Client::reset_const(Reply *rep) {
  out_rid = new_rid();
  int cur_timeout = atimeout;

#ifdef ADAPTIVE_TIMER
  // WARNING Not TESTED
  if (creset) {
    cur_timeout = (atimeout+ctimer.elapsed()/(clock_mhz*1000));
  }
  else {
    cur_timeout = atimeout;
  }
  creset = false;
  ctimer.reset();
#endif
 
  //retransmit_commit_timer->adjust(cur_timeout);
  retransmit_request_timer->stop();
  start_commit_timer->stop();
  //retransmit_commit_timer->stop();
  retransmit_reply_timer->stop();

  // Re-initialize 
  out_req = 0;
  cur_state = No_out_request_state;
  
	read_only_opt = false;//从下面接着添的

  /*if ((lc_cache != NULL)) {
    delete(lc_cache);
    lc_cache = NULL;
    lc_complete = false;
    read_only_opt = false;
  }*///注释掉
  
  // Choose view in returned rep. TODO: could make performance
  // more robust to attacks by picking the median view in the
  // certificate.
  v = rep->view();
  cur_primary = v % num_replicas;
  	  
#ifdef ADJUST_RTIMEOUT
  // Same as PBFT
  // WARNING: Not tested 
  latency.stop();
  ctimeout = (3*ctimeout+
	      latency.elapsed()*Rtimeout_mult/(clock_mhz*1000))/4+1;
#endif
}
	  
bool Client::send_request(Request *req) {
  bool ro = req->is_read_only();
  if (out_req == 0) { 
    // Clear all previous replies certificate 
	mycl_reps->clear();
    //c_reps->clear();注释掉
    r_reps->clear();
    primary_req = NULL;

    // fprintf(stderr,"Sending request for %llu \n", out_rid);
    
    // Set commit opt flag
    if (commit_opt_flag) {
      // fprintf(stderr,"Sending request for with opt flag set \n"); 
      req->set_commit();
    }

    // AUthenticate the request 
    req->authenticate();
    // Send request to service
    if (ro || req->size() > Request::big_req_thresh) {
      // read-only requests and big requests are multicast to all replicas.
      send(req, All_replicas);
    } else {
      // read-write requests are sent to the primary only.
      send(req, primary());
    }
    out_req = req;
    need_auth = false;
    n_retrans = 0;


#ifdef ADJUST_RTIMEOUT
    // Adjust timeout to reflect average latency
    retransmit_request_timer->adjust(rtimeout);

    // Start timer to measure request latency
    latency.reset();
    latency.start();
#endif

    retransmit_request_timer->start();
    cur_state = Sent_request_state; // sent-request
    return true;

  } else {
    // Another request is being processed.
    return false;
  }
}

void Client::check_commit_opt() {
  
  // Set the commit optimization flag if we do not hear from all the replicas
  Message* rm = NULL;

  while (!r_reps->max_match()) {
    Reply* rep=(Reply *)NULL;
    rm = recv_non_block();
    if ((rm != NULL) && (rm->tag() == Reply_tag)) {
      if (!Reply::convert(rm, rep) || rep->request_id() != out_rid || !rep->verify()) {
	//fprintf(stderr,"Failed to read response with rid :%llu cur rid :%llu cid : %d \n", 
	//rep->request_id(),out_rid,rep->id());
	delete rm;
	continue;
      }

      // Add the response 
      if(r_reps->add(rep) && r_reps->max_match()) {
	commit_opt_flag = false;
	break;		    
      }       
      else {
	continue;
      }
    }
    
    if ((rm == NULL) && (!r_reps->max_match())) { 
      commit_opt_flag = true;
      break;
    }		  
  }
  if (r_reps->max_match()) {
    commit_opt_flag = false;
  }
}

 //
 //
 // ZYZZYVA : Client state machine : 
 // 1. request-sent (RS) : request is sent and conservative-retransmit-timer is started
 //     - if 2f+1 OR messages are matching: stop crt timer and go to step 2 
 //     - Timeout : retransmit request if fewer than 2f+1 OR (ordered-request) are recived
 //     - Timeout : Trigger commit phase (step 3) if at least 2f+1 OR messages are received but 
 //                 less than 2f+1 messages are matching  
 // 2. request-ordered (RO) : start the aggressive-start-commit timer
 //     - received 3f+1 matching OR messages : stop the asc timer and go to step 4
 //     - Timeout : stop the aggressive-start-commit timer and go to step 3 
 //    
 // 3. commit-started (CS) : Start the commit phase by broadcasting LC requests and 
 //                     start the retransmit-commit-request
 //      - received 3f+1 matching OR messages or 2f+1 matching CL (committed-locally) messages :
 //        go to step 4  
 //      - Timeout : retransmit LC (local-commit) request 
 //
 // 4. request-committed (RC): Start reply-timer and wait for f+1 matching responses    
 //      - if f+1 matching reply messages : stop the timer and go to 5.
 //      - Timeout : broadcast retransmit-reply messages to the replicas
 //
 // 5. reply-received(RR) : if request-committed is true and it receives f+1 matching replies

 // TO DO :  Adapt client timers to reduce latency. How do we regulate when to start commit phase?
 //           On one hand we cannot be aggressive in starting local-commit phase (as soon as we 
 //           receive 2f+1 matching ORs) as may be doing more work in sending local-commit 
 //           request which could have been avoided if we were to wait some more time to allow
 //           f other servers to respond. At the same time, we 
 //           cannot wait too long to receive 3f+1 ORs from all replicas as some of them can be 
 //           too slow or faulty. Hence, we use following algo. to adapt start-commit-timer.
 //         (1) Start with aggressive start-commit-timer (t_a) = min_value;
 //         (2) Adapt the t_a(t+1) of the next request using the following algorithm :
 //              - Case 1 : Gracious execution completes before the end of commit phase
 //                         but after the start of commit phase
 //                        -- Measure the time it took to receive 3f+1 Order-requests (say t_3f+1) if 
 //                           it receives 3f+1 Order-requests before the end of commit-phase. Use this 
 //                           to reset the aggressive start-commit-timer(t_a).
 //                           t_a(t+1) = t_a(t) + (t_3f+1(t) - t_commit(t))
 //                           where t_commit(t) is the time it took to start the commit phase in the previous request
 //             - Case 2 : Gracious execution ends before the start of commit phase
 //                        -- t_a(t+1) = t_3f+1(t) - t_2f+1(t) + min_value
 //                           where t_2f+1(t) is the time taken to receive 2f+1 OR messsages for previous request
 //             - Case 3 : Gracious execution does not end before the end of commit phase
 //                        -- Reduce t_a(t+1) aggressively (as some replicas may be slow or faulty)
 //                        -- t_a(t+1) = max(t_a(t)/2,min_value)

Reply *Client::recv_reply() {

  if (out_req == 0)
    // Nothing to wait for.
    return 0;

  //
  // Wait for reply
  // 
  
  while (1) {
    Message* m = recv();
    bool ro = out_req->is_read_only();
    Reply* rep=(Reply *)NULL;
	
    // If current request is not read-only
    if (!ro) {
      
    // 1.0 Based on the mag type and current state, take action 
    switch (m->tag()) {

      case Reply_tag: // Speculative reply 
       {
        // It has both order information and also application reply

	// Drop if it is not the current request or if it is not from 
	// an execution replica or certificate is complete
	if (!Reply::convert(m, rep) || rep->request_id() != out_rid || !rep->verify()) {
	  //fprintf(stderr,"Failed to read response with rid :%llu cur rid :%llu cid : %d \n", 
	  //rep->request_id(),out_rid,rep->id());
	  delete m;
	  continue;
	}

	// Add the response 
	if(!r_reps->add(rep)) {
	  //fprintf(stderr,"Failed to add response with rid :%llu cid : %d Count : %d recvd : %d\n", 
	  //   rep->request_id(),rep->id(),r_reps->num_correct(), r_reps->num_elements() );
        }
	else {
	  //fprintf(stderr,"Added response with rid :%llu cid : %d Count : %d recvd : %d\n", 
	  //rep->request_id(),rep->id(),r_reps->num_correct(), r_reps->num_elements());
	}
	
	// Return the response 
	// 0. If it is in request committed state (waiting for full reply).
	// 1. For Zyzzyva (3f+1 replicas):
	//    Wait until all the replies match or 2f+1 match but all replicas locally committed
	//    the request
	// 2. For Zyzzyva5 (5f+1 replicas): 
	//    Wait until 4f+1 replies match (they need not locally commit)
        
        if ((cur_state == Request_committed_state) || 
	    (r_reps->max_match()) ||
	    (r_reps->is_complete() && ((prot_5f == 1) || (r_reps->is_locally_committed())))) {
	  rep = app_reply(); // Check if there is a full reply
	  if (rep) {

	    if (cur_state == Local_commit_state) {

#ifdef ADAPTIVE_TIMER
	      ctimer.stop();
	      creset = true;
#endif
	      // 改 注释掉 ((Client*)node)->retransmit_commit_timer->stop();	      
	    }

	    // Update commit opt flag for Zyzzyva (not Zyzzyva5)
	    if ((prot_5f != 1) && !r_reps->max_match()) {
	      check_commit_opt();
	    }

            reset_const(rep); // reset all the local state variables
	    
	    // Also reset the local commit timer
	    start_local_commit = false;

	    //fprintf(stderr,"Done request with rid :%llu \n", rep->request_id());
	    return rep;
	  }
	}
        else if (r_reps->is_recv_complete()) {
	  // Move this retransmit-request handler
	  // if we receive 2f+1 OR requests (even when they don't match), start commit phase after the timer
	  //fprintf(stderr,"Recv complete ...\n");
	  if (cur_state == Sent_request_state) {
	    //fprintf(stderr,"Recv complete start commit timer...\n");
	    ((Client*)node)->retransmit_request_timer->stop();
	    if (start_local_commit) {}
		/*改 注释掉{((Client*)node)->send_local_commit();}
	    ((Client*)node)->retransmit_commit_timer->start();*/
	  }
	  cur_state = Local_commit_state;
        } 
	else {
	}
       }
       break;

    case Mycommitted_locally_tag:
       {
        // 1. enqueue the messages in the committed-locally certificate
	// 2. if the current state is Local_commit_state and receive 2f+1 matching committed-locally messages then   
	//      -- stop the retransmit-commit timer
	//      -- return app. reply if we have (f+1) matching app. responses 
	//      -- else set state to "Request_committed_state" and wait for app replies
	Mycommitted_locally *mycl_req= (Mycommitted_locally *)NULL;

	// Drop if it is not a valid request or the current request or if I haven't started the commit phase
	if (!Mycommitted_locally::convert(m, mycl_req) || mycl_req->request_id() != out_rid
	    || (cur_state == Sent_request_state) || (cur_state == Request_committed_state)) {
	  delete m;
	  continue;
	}

	// Check if I received 2f+1 committed-locally responses from the replicas
	if (!mycl_reps->add(mycl_req)) {
	  //fprintf(stderr,"Failed to add CL with rid :%llu cid : %d cert count : %d \n", 
	  //c_req->request_id(),c_req->id(),c_reps->num_correct());
        } 
	else {
	  //fprintf(stderr,"Added CL with rid :%llu cid : %d cert count : %d \n", 
	  //   c_req->request_id(),c_req->id(),c_reps->num_correct());
	}

	if (mycl_reps->is_complete()) { 
	  if (cur_state == Local_commit_state) {
#ifdef ADAPTIVE_TIMER
	      ctimer.stop();
	      creset = false;
#endif	
	  
          //retransmit_commit_timer->stop();//改 注释掉...

	      rep = app_reply();
	      if (rep) {
		// Update commit opt flag for Zyzzyva (not Zyzzyva5)
		if ((prot_5f != 1) && !r_reps->max_match()) {
		  check_commit_opt();
		}
		reset_const(rep);
		//fprintf(stderr,"Done request with rid :%llu after CL \n", rep->request_id());
		return rep;
	      }
	      else {
		// request is locally committed at 2f+1 replicas but the
		// client failed to receive the full reply due to optimization
		// So send a reply_request to replicas
		if (cur_state != Request_committed_state) {
		  retransmit_reply_timer->start();
		}
	      }
	      cur_state =  Request_committed_state;      
	  }
	}  
       }
       break;

      default:
	delete m;
	break;
      }
    }
    else {
      // It is a read-only request then just wait for the response 
      switch (m->tag()) {

      case Reply_tag: // Application reply  
      {
			Reply* rep=(Reply *)NULL;
       
			// Drop if it is not the current request or if it is not from 
			// an execution replica or certificate is complete
			if (!Reply::convert(m, rep) || rep->request_id() != out_rid) {
			  delete m;
			  continue;
			}

		if(!r_reps->add(rep)) {
		  //fprintf(stderr,"Could not add response from id : %d rid :%llu \n", rep->id(), rep->request_id());
		}
	  
			if ((r_reps->max_match()) ||
			(r_reps->is_complete() && ((prot_5f == 1) || (r_reps->is_locally_committed())))) {
		  rep = app_reply();
		  if (rep) {
				reset_const(rep); // reset all the local state variables
			// fprintf(stderr,"Received reply for %llu \n", rep->request_id());
			return rep;
		  }   
		}
      }
       break;
	
      default :
	delete m;
	break;
      }
    }
  }
}

// Retransmit request if fewer than 2f+1 LC messages are received
void retransmit_request_handler() {
  th_assert(node, "Client is not initialized");
  // Retransmit request only when it does not receive 
  // 2f+1 OR responses
  //fprintf(stderr,"Retransmitting request for %llu \n", ((Client*)node)->get_rid());
  ((Client*)node)->retransmit();
  ((Client*)node)->retransmit_request_timer->restart();
}

// Start commit phase
// Change state to 3 and send the "local-commit" message
void start_commit_handler() {
  th_assert(node, "Client is not initialized");
#ifdef ADAPTIVE_TIMER
	    ((Client*)node)->start_ctimer();
#endif
  /*if (((Client*)node)->send_local_commit()) {
    ((Client*)node)->retransmit_commit_timer->start();
	//这个计时器应该设置在repicas中 注意什么时间开始、什么时间结束
  }*/
}

// Retransmit local-commit message 整段注释掉
/*void retransmit_commit_handler() {
  th_assert(node, "Client is not initialized");
  ((Client*)node)->send_local_commit();
  ((Client*)node)->retransmit_commit_timer->adjust(((Client*)node)->c_timeout());
  ((Client*)node)->retransmit_commit_timer->restart();
}*/

// Retransmit pull_reply
void retransmit_reply_handler() {
  th_assert(node, "Client is not initialized");
  ((Client*)node)->retransmit(); // retransmit request
  ((Client*)node)->retransmit_reply_timer->restart();
}

// TO DO: Use speculative replies as well as order request messages instead of order request message 
//不用要这个send_local_commit函数
bool Client::check_replies() {
  //fprintf(stderr, "send local commit \n");

  if (cur_state == Request_committed_state) {
    // If request is committed nothing to do 
    return false;
  }
  
  if (cur_state == Sent_request_state) {
    cur_state = Local_commit_state;
  }
  
  // Check if we received n-f order-request messages 
  assert(r_reps->is_recv_complete());
 
  // Find out if the order-request certificate is complete
  // (that is we received 2f+1 matching order-request messages)
  bool complete = r_reps->is_complete();

  // Construct the message if not in cache 
  struct iovec *lc_msg;
  int len;
  int psize;
  //int msize=0;
  //fprintf(stderr, "send local commit ....\n");
  if (complete) {
	  bool check = r_reps->collect_proof(lc_msg, len, psize);
      assert (check == true);
		// Check if there is a LC message with a complete certificate
		// If not, create the LC message and cache it
		//fprintf(stderr, "send local commit, complete ..\n");

		/* if ((lc_cache == NULL) || (lc_complete == false))  {
      // Create the LC message 
      
      // Collect the proof
      bool check = r_reps->collect_proof(lc_msg, len, psize);
      assert (check == true);
      //fprintf(stderr, "send local commit, creating local commit msg ..\n");

      // Create local-commit message and Fill the PREAMBLE 
     /* Local_commit *lc = new Local_commit(true, psize, len,(Order_request_rep *)lc_msg[0].iov_base);
      lc->append_proof(lc_msg,len,psize);
      lc->authenticate(true);
      free(lc_msg);

      // Delete previous lc_cache
      if (lc_cache != NULL) {
	delete(lc_cache);
      }

      lc_cache = lc;
      lc_complete = true;

      msize = psize+lc->size(); */
      //fprintf(stderr, "Sending local commit message : %d \n",((Order_request_rep *)lc_msg[0].iov_base)->seq_num); */
    }
  
  else {
    /*if (lc_cache == NULL)  {
      // Collect the proof
      bool check = r_reps->collect_proof(lc_msg,len,psize);
      assert (check == true);
    
      // Create local-commit message and Fill the PREAMBLE with Incomplete certificate
      Local_commit* lc = new Local_commit(false,psize,len,NULL);
      lc->authenticate(true);
      lc->append_proof(lc_msg,len,psize);
      free(lc_msg);

      // Cache the message to reuse on retransmissions 
      if (lc_cache != NULL) {
	delete(lc_cache);
      }
      lc_cache= lc;
      lc_complete = false; 
    
      msize = psize+lc->size(); 
    }*/
  }

  // Set this to true so that next request starts the local commit phase immediately
  start_local_commit = true;//开启local_commit定时器

  // Send LC message to the replicas using scatter-gather to
  // reduce overhead
  //node->send(lc_cache,All_replicas);
  //node->send_msg(lc_cache.msg, lc_cache.len, msize, All_replicas);

  return true;
}

void Client::retransmit() {
  // Retransmit any outstanding request.
  static const int thresh = 2;

  if (out_req != 0) {
    INCR_OP(req_retrans);

    //    fprintf(stderr, ".");
    /*n_retrans++;
    if (n_retrans == nk_thresh || n_retrans % nk_thresh_1 == 0) {
      send_new_key();
    }*/

    bool ro = out_req->is_read_only();
    bool change = (ro || out_req->replier() >= 0) && n_retrans > thresh;
    //printf("%d %d %d %d\n", id(), n_retrans, ro, out_req->replier());

    if (need_auth || change) {
      // Compute new authenticator for request
      out_req->re_authenticate(change);
      need_auth = false;
      if (ro && change) r_reps->clear();
    }

    if (out_req->is_read_only() || n_retrans > thresh 
	|| out_req->size() > Request::big_req_thresh) {
      // read-only requests, requests retransmitted more than
      // mcast_threshold times, and big requests are multicast to all
      // replicas.
      send(out_req, All_replicas);
    } else {
      // read-write requests are sent to the primary only.
      send(out_req, primary());
    }
  }
}

void Client::send_new_key() {
  Node::send_new_key();
  need_auth = true;

  // Cleanup reply messages authenticated with old keys.
  r_reps->clear();
  mycl_reps->clear();//改
}
