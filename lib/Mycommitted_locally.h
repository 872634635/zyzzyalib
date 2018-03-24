
#ifndef _Mycommitted_locally_h
#define _Mycommitted_locally_h 1

#include "types.h"
#include "Message.h"
#include "Digest.h"

class Principal;
class Myc_info;

// 
// Mycommitted_locally messages have the following format.
//
struct Mycommitted_locally_rep : public Message_rep {
  View v;                // current view
  Request_id rid;        // unique request identifier
  Digest rh_digest;      // request history digest
  int replica;           // id of replica sending the reply

  // Followed by a MAC authenticating that request is committed locally
};

class Mycommitted_locally : public Message {
  // 
  // Committed_locally messages
  //
public:
  Mycommitted_locally() : Message() {}

  Mycommitted_locally(Mycommitted_locally_rep *r);

  Mycommitted_locally(View view, Request_id req, int replica, Digest &rh_d, Principal *p);
  // Return reply to "clientp" with history digest "rh_d", reply digest, "rep_d"
  // request rid "req"

  void authenticate(Principal *p);//
  // Effects: Terminates the construction of a reply message by
  // setting the length of the reply to "act_len", appending a MAC,
  // and trimming any surplus storage.
  
  View view() const;
  // Effects: Fetches the view from the message

  Request_id request_id() const;
  // Effects: Fetches the request identifier from the message.

  int id() const;
  // Effects: Fetches the reply identifier from the message.

  Digest &request_history_digest() const;
  // Effects : Fetches the request-history digest from the message

  bool verify();
  // Effects: Verifies if the message is authenticated by rep().replica.

  bool match(Mycommitted_locally *r);//
  // Effects: Returns true if the replies match.

  
   static bool convert(Message *m1, Mycommitted_locally *&m2);
  // Effects: If "m1" has the right size and tag of a "Mycommitted_locally", casts
  // "m1" to a "Committed_locally" pointer, returns the pointer in "m2" and
  // returns true. Otherwise, it returns false. Convert also trims any
  // surplus storage from "m1" when the conversion is successfull.

private:
  
	Mycommitted_locally_rep &rep() const;
  // Effects: Casts "msg" to a Committed_locally_rep&
  
};

inline Mycommitted_locally_rep& Mycommitted_locally::rep() const { 
  th_assert(ALIGNED(msg), "Improperly aligned pointer");
  return *((Mycommitted_locally_rep*)msg); 
}
   
inline View Mycommitted_locally::view() const { return rep().v; }

inline Request_id Mycommitted_locally::request_id() const { return rep().rid; }

inline int Mycommitted_locally::id() const { return rep().replica; }

inline Digest& Mycommitted_locally::request_history_digest() const { return rep().rh_digest; }

  
inline bool Mycommitted_locally::match(Mycommitted_locally *r) {
  return ((rep().rh_digest == r->rep().rh_digest) &&  (view() == r->view()) && (rep().rid == r->rep().rid));
}


#endif // _Mycommitted_locally_h


