#ifndef _Replica_h
#define _Replica_h 1

#include "types.h"
#include "Req_queue.h"
#include "Log.h"
#include "Set.h"
//#include "Local_commit.h"
//#include "Committed_locally.h"
#include "Mycommit.h"//改
#include "Mycommitted_locally.h"//改

#include "Certificate.h"
#include "Certificate_order.h"
#include "Prepared_cert.h"
#include "View_info.h"
#include "Rep_info.h"
#include "Myc_info.h"//改
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


class Request;
class Reply;
//class Local_commit;
//class Committed_locally;
class Mycommit;//改
class Mycommitted_locally;//改

class Order_request;
class Checkpoint;
class Status;
class View_change;
class New_view;
class New_key;
class Reply;

extern void vtimer_handler();
extern void stimer_handler();

#define ALIGNMENT_BYTES 2

class Replica : public Node {
public:

#ifndef NO_STATE_TRANSLATION

  Replica(FILE *config_file, FILE *config_priv, int num_objs,
	  int (*get_segment)(int, char **),
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

  void process_new_view(Seqno min, Digest d, Seqno max, Seqno ms);
  // Effects: Update replica's state to reflect a new-view: "min" is
  // the sequence number of the checkpoint propagated by new-view
  // message; "d" is its digest; "max" is the maximum sequence number
  // of a propagated request +1; and "ms" is the maximum sequence
  // number known to be stable.

  void send_view_change();
  // Effects: Send view-change message.

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


private:
  friend class State;

  //
  // Message handlers:
  //
  void handle(Request* m);
  void handle(Order_request* m);
  void handle(Local_commit* m);
  void handle(Mycommit* m);//改
  void handle(Checkpoint* m);
  void handle(View_change* m);
  void handle(New_view* m);
  void handle(View_change_ack* m);
  void handle(Status* m);
  void handle(New_key* m);
  void handle(Reply* m, bool mine=false);
  // Effects: Execute the protocol steps associated with the arrival
  // of the argument message.  


  friend void vtimer_handler();
  friend void stimer_handler();
  // Effects: Handle timeouts of corresponding timers.

  //
  // Auxiliary methods used by primary to send messages to the replica
  // group:
  //
  void send_order_request();
  // Effects: Sends a Pre_prepare message		

  void send_committed_locally(Local_commit *m, Digest &d);
  // Effects: Sends a prepare message if appropriate.

  void send_mycommitted_locally(Mycommit *m, Digest &d);//改 这里的d是rh_d
  // Effects: Sends a prepare message if appropriate.//改

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

//不知道有用没
#ifndef NO_STATE_TRANSLATION
  char *myc_info_mem();
  // Returns: pointer to the beggining of the mem region used to store the 
  // mycommits
#endif

  void join_mcast_group();
  // Effects: Enables receipt of messages sent to replica group

  void leave_mcast_group();
  // Effects: Disables receipt of messages sent to replica group

  void try_end_recovery();
  // Effects: Ends recovery if all the conditions are satisfied

  //
  // Instance variables:
  //
  Seqno seqno;       // Sequence number to attribute to next protocol message,                      
                     // only valid if I am the primary.
  static int const congestion_window = 1;
  
  Seqno checkpoint_stable; // Sequence number of for which checkpoint is stable
  Seqno last_stable;   // Sequence number of last stable state.
  Seqno low_bound;     // Low bound on request sequence numbers that may
                       // be accepted in current view.

  Seqno max_request_ordered; // Sequence number of highest prepared request
 // Seqno max_committed_locally; // Sequence number of highest prepared request
 Seqno max_mycommitted_locally; // Sequence number of highest prepared request
  Seqno last_executed; // Sequence number of last executed message.
  Seqno last_mycommit; // 改Sequence number of last mycommit send.
  
  int batch_size; // batch_size 

  // Sets and logs to keep track of messages received. Their size
  // is equal to max_out.
  Request_map rqueue;    // For read-write requests.
  Req_history_log rh_log; // Request history log

  //Log<Certificate<Local_commit> > clog; // For local-commit queue
  Log<Certificate<Mycommit> > myclog; // For mycommit queue改？？？？？
  Log<Certificate<Order_request> > olog;

  Log<Certificate<Checkpoint> > elog;

  // Set of stable checkpoint messages above my window.
  Set<Checkpoint> sset;

  // Last replies sent to each principal.
  Rep_info replies;
  //Myc_info myconnits;//改
  Req_cache requests;

  // State abstraction manages state checkpointing and digesting
  State state;

  ITimer *stimer;  // Timer to send status messages periodically.
  Time last_status; // Time when last status message was sent

  // 
  // TO DO :View changes:
  //
  View_info vi;   // View-info abstraction manages information about view changes
  ITimer *vtimer; // View change timer
  bool limbo;     // True iff moved to new view but did not start vtimer yet.
  bool has_nv_state; // True iff replica's last_stable is sufficient
                     // to start processing requests in new view.

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

inline  bool Replica::has_new_view() const {
  return true ;
}

template <class T> inline void Replica::gen_handle(Message *m) {
  T *n;                
  if (T::convert(m, n)) {
    handle(n);           
  } else {               
    delete m;
  } 
}


inline bool Replica::delay_vc() {
  return true;
}

#ifndef NO_STATE_TRANSLATION
inline char* Replica::rep_info_mem() {
  return NULL;
}
#endif

#ifndef NO_STATE_TRANSLATION
inline   char replica::*myc_info_mem(); {
  return NULL;
}
#endif

#endif //_Replica_h
