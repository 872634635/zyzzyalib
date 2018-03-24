//Mycommitted_locally.cc文件

#include <strings.h>
#include "th_assert.h"
#include "Message_tags.h"
#include "Mycommitted_locally.h"
#include "Node.h"
#include "Principal.h"

#include "Statistics.h"

Mycommitted_locally::Mycommitted_locally(Mycommitted_locally_rep *r) : Message(r) {}


Mycommitted_locally::Mycommitted_locally(View view, Request_id req, int replica, Digest &d,
	     Principal *p) : Message(Mycommitted_locally_tag, sizeof(Mycommitted_locally_rep)+MAC_size) {
    rep().v = view; 
    rep().rid = req;
    rep().replica = replica;
    rep().rh_digest = d;

    //INCR_OP(mycommit_auth);//怎么改
    //START_CC(mycommit_auth_cycles);//??
    p->gen_mac_out(contents(), sizeof(Mycommitted_locally_rep), contents()+sizeof(Mycommitted_locally_rep));
   // STOP_CC(mycommit_auth_cycles);
    set_size(sizeof(Mycommitted_locally_rep)+MAC_size);
}


void Mycommitted_locally::authenticate(Principal *p) {//这是要发给client或者说mycommitted_locally是要发给client
  int old_size = sizeof(Mycommitted_locally_rep);

  INCR_OP(mycommit_auth);
  START_CC(mycommit_auth_cycles);
  p->gen_mac_out(contents(), old_size, contents()+old_size);//
  STOP_CC(mycommit_auth_cycles);
  set_size(old_size+MAC_size);
}


bool Mycommitted_locally::verify() {

  // Check signature.
  Principal *replica = node->i_to_p(rep().replica);
  
  INCR_OP(mycommit_auth_ver);
  START_CC(mycommit_auth_ver_cycles);

  bool ret = replica->verify_mac_in(contents(), sizeof(Mycommitted_locally_rep), contents()+sizeof(Mycommitted_locally_rep));

  STOP_CC(mycommit_auth_ver_cycles);

  return ret;
}


bool Mycommitted_locally::convert(Message *m1, Mycommitted_locally *&m2) {
  if (!m1->has_tag(Mycommitted_locally_tag, sizeof(Mycommitted_locally_rep)))
    return false;
  
  m1->trim();
  m2 = (Mycommitted_locally*)m1;
  return true;
}