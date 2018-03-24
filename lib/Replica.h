#ifndef _Replica_h
#define _Replica_h 1

#include "types.h"
#include "Req_queue.h"
#include "Log.h"
#include "Set.h"


#include "Mycommit.h"	//改
#include "Mycommitted_locally.h"	//改
#include "Certificate.h"
#include "Certificate_order.h"
#include "Prepared_cert.h"
#include "View_info.h"
#include "Rep_info.h"
#include "Req_cache.h"
#include "Stable_estimator.h"
#include "Partition.h"
#include "Digest.h"
#include "Node.h"
#include "State.h"
#include "libbyz.h"
#include "State_defs.h"
#include "Request_map.h"
#include "Request_history.h"
#include "Hate_primary.h"
#include "Checkpoint.h"

class Request;
class Reply;
class Mycommit;	//改
class Mycommitted_locally;	//改
class Order_request;
class Checkpoint;
class Status;
class View_change;
class New_view;
class New_key;
class Reply;

extern void itimer_handler();
extern void vtimer_handler();
extern void stimer_handler();

#define ALIGNMENT_BYTES 2

class Replica : public Node {
public:

#ifndef NO_STATE_TRANSLATION

  Replica(FILE *config_file, FILE *config_priv, int num_objs,
	  int (*get_segment)(int, char **),//函数指针
	  void (*put_segments)(int, int *, int *, char **),
	  void (*shutdown_proc)(FILE *o),
	  void (*restart_proc)(FILE *i),
	  short port=0, int bsize=1);

  // Effects: Create a new server replica using the information in
  // "config_file" and "config_priv". The replica's state is set has
  // a total of "num_objs" objects. The abstraction function is
  // "get_segment" and its inverse is "put_segments". The procedures
  // invoked before and after recovery to save and restore extra
  // state information are "shutdown_proc" and "restart_proc".

#else
  Replica(FILE *config_file, FILE *config_priv, char *mem, int nbytes);
  // Requires: "mem" is vm page aligned and nbytes is a multiple of the
  // vm page size.
  // Effects: Create a new server replica using the information in
  // "config_file" and "config_priv". The replica's state is set to the
  // "nbytes" of memory starting at "mem". 
#endif

  virtual ~Replica();
  // Effects: Kill server replica and deallocate associated storage.

  void recv();
  // Effects: Loops receiving messages and calling the appropriate
  // handlers.

  // Methods to register service specific functions. The expected
  // specifications for the functions are defined below.
  void register_exec(int (*e)(Byz_req *, Byz_rep *, Byz_buffer *, int, bool));
  // Effects: Registers "e" as the exec_command function. 

#ifndef NO_STATE_TRANSLATION
  void register_nondet_choices(void (*n)(Seqno, Byz_buffer *), int max_len,
			       bool (*check)(Byz_buffer *));
  // Effects: Registers "n" as the non_det_choices function and "check" as
  //          the check_non_det check function.
#else
  void register_nondet_choices(void (*n)(Seqno, Byz_buffer *), int max_len);
  // Effects: Registers "n" as the non_det_choices function.
#endif

  void compute_non_det(Seqno n, char *b, int *b_len);
  // Requires: "b" points to "*b_len" bytes.  
  // Effects: Computes non-deterministic choices for sequence number
  // "n", places them in the array pointed to by "b" and returns their
  // size in "*b_len".

  int max_nd_bytes() const;
  // Effects: Returns the maximum length in bytes of the choices
  // computed by compute_non_det

  int used_state_bytes() const;
  // Effects: Returns the number of bytes used up to store protocol
  // information.

#ifndef NO_STATE_TRANSLATION
  int used_state_pages() const;
  // Effects: Returns the number of pages used up to store protocol
  // information.
#endif

#ifdef NO_STATE_TRANSLATION
  void modify(char *mem, int size);
  // Effects: Informs the system that the memory region that starts at
  // "mem" and has length "size" bytes is about to be modified.
#else
  void modify_index_replies(int bindex);
  // Effects: Informs the system that the replies page with index
  // "bindex" is about to be modified.
#endif

  void modify_index(int bindex);
  // Effects: Informs the system that the memory page with index
  // "bindex" is about to be modified.

  void add_or_msg(Seqno s, Reply *rep);

  void process_new_view(Seqno min, Seqno max_committed, Seqno max);
  // Effects: Update replica's state to reflect a new-view: "min" is
  // the sequence number of the checkpoint propagated by new-view
  // message; "d" is its digest; "max" is the maximum sequence number
  // of a propagated request +1; and "ms" is the maximum sequence
  // number known to be stable.

  void send_view_change();
  // Effects: Send view-change message.

  void send_hate_primary();
  // Effects: Send hate primary message

  void send_status(Status *st);
  // Effects: Sends a status message.

  void send_checkpoint(Seqno c);
  // Send checkpoint 

  bool shutdown();
  // Effects: Shuts down replica writing a checkpoint to disk.

  bool restart(FILE* i);
  // Effects: Restarts the replica from the checkpoint in "i"

  bool has_req(int cid, Digest &d);
  // Effects: Returns true iff there is a request from client "cid"
  // buffered with operation digest "d". XXXnot great

  bool delay_vc();
  // Effects: Returns true iff view change should be delayed.

 
  Seqno last_stable_chkpt(); // return last stable
  Seqno last_executed_req(); // return last ordered
  Seqno last_mycommit_req(); // 改return last mycommit
  Seqno last_mycommitted(); // 改return last committed

  

private:
  friend class State;

  //
  // Message handlers:
  //
  void handle(Request* m);
  void handle(Order_request* m);
  void handle(Mycommit* m);	//改
  void handle(Checkpoint* m);
  void handle(View_change* m);
  void handle(New_view* m);
  void handle(View_change_ack* m);
  void handle(Status* m);
  void handle(New_key* m);
  void handle(Reply* m, bool mine=false);
  void handle(Hate_primary *m);
  // Effects: Execute the protocol steps associated with the arrival
  // of the argument message.  


  friend void itimer_handler();
  friend void vtimer_handler();
  friend void stimer_handler();
  //friend void start_commit_handler();//改 什么时间打开该定时器
 
 
  
  // Effects: Handle timeouts of corresponding timers.

  //
  // Auxiliary methods used by primary to send messages to the replica
  // group:
  //
  void send_order_request(bool retransmitted);
  // Effects: Sends a Pre_prepare message	
  
  void send_mycommit(Order_request *m, Digest &d);//改
  void send_mycommitted_locally(Mycommit *m, Digest &d);//改
  
  //void send_committed_locally(Local_commit *m, Digest &d);注释掉关于该committed_locally消息的处理
  // Effects: Sends a prepare message if appropriate.

  void send_null();
  // Send a pre-prepare with a null request if the system is idle

  // 
  // Miscellaneous:
  //
  void execute_read_only(Request *m);
  // executes the request 

  void execute_request(Seqno s, Request *r);
  // Execute request now or enquque request

  void execute_request_speculatively(Seqno num);
  // Execute the outstanding requests in the request history log less than num 
 
  void enqueue_request();
  // Effects: Executes as many commands as possible by calling
  // execute_prepared; sends Checkpoint messages when needed and
  // manipulates the wait timer.

  void mark_stable(Seqno seqno);
  // Requires: Checkpoint with sequence number less than "seqno" is stable.
  // Effects: Marks it as stable and garbage collects information.
  // "have_state" should be true iff the replica has a the stable
  // checkpoint.

  void new_state(Seqno seqno);
  // Effects: Updates this to reflect that the checkpoint with
  // sequence number "seqno" was fetch.
  
  bool time_to_checkpoint(Seqno i,Seqno bnum, Seqno last_stable);
  // Checks if it is time to checkpoint request with seqno i and batch_size bnum-i+1
    
  void recover();
  // Effects: Recover replica.

  bool has_new_view() const; 
  // Effects: Returns true iff the replica has complete new-view
  // information for the current view.

  template <class T> bool in_w(T *m);
  // Effects: Returns true iff the message "m" has a sequence number greater
  // than last_stable and less than or equal to last_stable+max_out.

  template <class T> bool in_wv(T *m);
  // Effects: Returns true iff "in_w(m)" and "m" has the current view.

  template <class T> void gen_handle(Message *m); 
  // Effects: Handles generic messages.

  template <class T> void retransmit(T *m, Time &cur, 
				     Time *tsent, Principal *p);
  // Effects: Retransmits message m (and re-authenticates it) if
  // needed. cur should be the current time.

  bool retransmit_rep(Reply *m, Time &cur,  
			 Time *tsent, Principal *p);

  void send_new_key();
  // Effects: Calls Node's send_new_key, adjusts timer and cleans up
  // stale messages.

  void enforce_bound(Seqno b);
  // Effects: Ensures that there is no information above bound "b".

  void enforce_view(View rec_view);
  // Effects: If replica is corrupt, sets its view to rec_view and
  // ensures there is no information for a later view in its.

  void update_max_rec();
  // Effects: If max_rec_n is different from the maximum sequence
  // number for a recovery request in the state, updates it to have
  // that value and changes keys. Otherwise, does nothing.

#ifndef NO_STATE_TRANSLATION
  char *rep_info_mem();
  // Returns: pointer to the beggining of the mem region used to store the 
  // replies
#endif

  void join_mcast_group();
  // Effects: Enables receipt of messages sent to replica group

  void leave_mcast_group();
  // Effects: Disables receipt of messages sent to replica group

  void try_end_recovery();
  // Effects: Ends recovery if all the conditions are satisfied

  Digest chkpt_digest(); 

  //
  // Upcalls into the application
  // 

  void exec_checkpoint(Seqno c);
  // TO DO: Upcall to the application to take a checkpoint

  void exec_checkpoint_stable(Seqno c);
  // TO DO: Upcall to the application to take make the checkpoint stable

  void exec_rollback(Seqno c);
  // TO DO: Upcall to the application to 
  //        rollback the execution state to checkpoint with seqno c
  
  void exec_rollforward(Seqno c);
  // TO DO: Upcall to the application to rollforward the execution state to c
  // This happens when this replica is far left behind compared to other replicas
  // and other replicas have garbage collected request information.

  // Last checkpoint digest

  //
  // Instance variables:
  //
  Seqno seqno;       // Sequence number to attribute to next protocol message,                      
                     // only valid if I am the primary.
  static int const congestion_window = 1;
  
  Seqno checkpoint_stable; // Sequence number of for which checkpoint is stable
  Seqno last_stable;   // Sequence number of last stable state.
  Seqno low_bound;     // Low bound on request sequxence numbers that may
                       // be accepted in current view.

  Seqno max_request_ordered; // Sequence number of highest prepared request
  Seqno max_mycommitted_locally; // Sequence number of highest prepared request
  Seqno last_executed; // Sequence number of last executed message.
  Seqno last_mycommit; //改  the seqno of  last mycommit send
  int batch_size; // cur batch_size 
  int max_batch_size; // batch_size 

  // Sets and logs to keep track of messages received. Their size
  // is equal to max_out.
  Request_map rqueue;    // For read-write requests.
  Req_history_log rh_log; // Request history log

 
  Log<Certificate<Order_request> > olog;

  Log<Certificate<Checkpoint> > elog;

  Log<Certificate<Hate_primary> > pflog; 

  // Set of stable checkpoint messages above my window.
  Set<Checkpoint> sset;

  //注意
  /*bool start_local_commit; // Flag to start local commit aggressively
  ITimer *start_commit_timer; */  // Aggressive start commit timer 
  
  // Last replies sent to each principal.
  Rep_info replies;
  Req_cache requests;

  
  // State abstraction manages state checkpointing and digesting
  State state;
  
  ITimer *stimer;  // Timer to send status messages periodically.
  Time last_status; // Time when last status message was sent

  // 
  // TO DO :View changes:
  //
  bool hate_primary;
  View hate_primary_view;
  ITimer *itimer;   // I HATE THE PRIMARY timer
  int wait_cid;     // Client id of the request which is not yet ordered
  int wait_rid;     // Requet id of the request that is not yet ordered

  View_info vi;   // View-info abstraction manages information about view changes
  ITimer *vtimer; // View change timer
  bool limbo;     // True iff moved to new view but did not start vtimer yet.
  bool has_nv_state; // True iff replica's last_stable is sufficient
                     // to start processing requests in new view.
 				 
   // Local-commit message cache



  Certificate_order<Mycommit> *myc_reps; //改。。。 Certificate with sending mycommitted_locally
  bool myc_complete;
  //
  // TO DO : Recovery
  //

  // Estimation of the maximum stable checkpoint at any non-faulty replica
 
  //
  // Pointers to various functions.
  //
  int (*exec_command)(Byz_req *, Byz_rep *, Byz_buffer *, int, bool);

  void (*non_det_choices)(Seqno, Byz_buffer *);
  int max_nondet_choice_len;

#ifndef NO_STATE_TRANSLATION
  bool (*check_non_det)(Byz_buffer *);
  int n_mem_blocks;
#endif
};

// Pointer to global replica object.
extern Replica *replica;


inline int Replica::max_nd_bytes() const { return max_nondet_choice_len; }

inline int Replica::used_state_bytes() const {
 return 0;
}

#ifndef NO_STATE_TRANSLATION
inline int Replica::used_state_pages() const {
 return 0;
}
#endif

#ifdef NO_STATE_TRANSLATION
inline void Replica::modify(char *mem, int size) {
  state.cow(mem, size);
}

#else

inline void Replica::modify_index_replies(int bindex) {
}
#endif

inline void Replica::modify_index(int bindex) {
}

inline Seqno Replica::last_stable_chkpt() { return last_stable;}
inline Seqno Replica::last_executed_req() { return last_executed;}
inline Seqno Replica::last_mycommitted() { return max_mycommitted_locally;}//改
inline Seqno Replica::last_mycommit_req() { return last_mycommit;}//改

inline  bool Replica::has_new_view() const {
  return v == 0 || (has_nv_state && vi.has_new_view(v));
}

template <class T> inline void Replica::gen_handle(Message *m) {
  T *n;                
  if (T::convert(m, n)) {
    handle(n);           
  } else {               
    delete m;
  } 
}

inline Digest Replica::chkpt_digest() {
  Digest d;
  if (last_stable > 0) {
    return elog.fetch(last_stable).cvalue()->digest();
  }
  else 
    return d;
}

inline bool Replica::delay_vc() {
  return true;
}

inline bool Replica::time_to_checkpoint(Seqno i,Seqno bnum, Seqno last_stable) {
	 return ((i <= (last_stable + checkpoint_interval)) &&
		 (bnum >= (last_stable + checkpoint_interval)));
}

#ifndef NO_STATE_TRANSLATION
inline char* Replica::rep_info_mem() {
  return NULL;
}
#endif


#endif //_Replica_h
