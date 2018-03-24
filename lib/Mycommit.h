/* 
类 Mycommit
< mycommit,req_history,client_ID，OR> //my commit 格式

replica在收到primary发过来的OR消息之后，如果验证通过，预执行该请求并发给client回应response。同时发mycommit消息给其他所有副本。
如果replica没有收到primary发的OR消息，但是收到f+1个其它副本发来的mycommit消息，如果commit消息中的oR一致，就执行相应的请求，并发mycommt给其他副本。
如果收到的mycommit（只要有一个）跟自己的OR不一致就发I hate the primary
如果收到的f+1个mycommit消息中与自己收到的OR不一致，触发viewchange。
收到2f+1个mycommit消息 完成local commit。

这里要有计数器和计时器，还有一个mycommit certificate 
如果收到OR消息就打开 计时器：规定时间内收到2f+1个mycommit消息 完成local commit。

只有收到一个mycommit消息就打开 计数器: 如果收到f+1个一致mycommit而没有收到primary发过来的OR消息，则立即执行该请求并发出mycommit给其他副本

*/
//整体搬砖reply.h reply.cc
//reply是要针对每一个request回复给client 而mycommit可以以batch为单位response

//要有一个tag代表mycommit消息

#ifndef _Mycommit_h
#define _Mycommit_h 1

#include "types.h"
#include "Message.h"
#include "Digest.h"
#include "Node.h"

class Principal;
class Myc_info;
class Node;


#define INCLUDE_ORDER_MSG 2
// mycommit message have the following format

struct Mycommit_rep : public Message_rep 
{
	View v; 				//Current view
	Request_id rid;			// unique request identifier作用 rid是client分配的
	Seqno seq_num;			//sequence number 
	Digest rh_digest;       // request history digest 
	int rep_id;             // id of replica sending the mycommit
	int b_size;              // batch size
	//1. Followed by order request sent by the primary 
	//    of size Order_request_rep + authenticator size
	// 2. Followed by the authenticator signed by the replica 

};

class Mycommit : public Message
{
	public:
	//多个 不同的构造函数作用是什么
	Mycommit() : Message() {}
	
	Mycommit(Mycommit_rep *myc);
	
	Mycommit(View view, Seqno seq_num,int bsize, int replica, Digest &rh_d, Principal *P);


	void authenticate();
	// Effects: Terminates the construction of a mycommit message by
	// setting the length of the mycommit to "act_len", appending a MAC,
	// and trimming any surplus storage.
	
 
	void re_authenticate(Principal *p);
	// Effects: Recomputes the authenticator in the mycommit using the most
	// recent key.
 
	View view() const;
	// Effects: Fetches the view from the message

	Seqno seqno() const;
	// Returns the seqno of the request 

	Request_id request_id() const;
	// Effects: Fetches the request identifier from the message.

	int rep_id() const;	//应该是发出该mycommit的replica的identifier
	// Effects: Fetches the mycommit identifier from the message.
	int id() const;//应该是发出该mycommit的replica的identifier
	// Effects: Fetches the mycommit identifier from the message.
	
	bool full() const;
	
	Digest &request_history_digest() const;
	// Effects : Fetches the request-history digest from the message
	 
	int bsize();
	// Return bsize
	
//	bool check_mylocally_committed();
  // Check if current reply is locally committed
	
	bool verify();
	// Effects: Verifies if the message is authenticated by rep().replica.

  
	bool match(Mycommit *myc);
	// Effects: Returns true if the mycommits match.
	
	static bool convert(Message *m1, Mycommit *&m2);//作用？？ 函数指针
	// Effects: If "m1" has the right size and tag of a "Mycommit", casts
	// "m1" to a "Mycommit" pointer, returns the pointer in "m2" and
	// returns true. Otherwise, it returns false. Convert also trims any
	// surplus storage from "m1" when the conversion is successfull.

	private:
	Mycommit_rep &rep() const;
	// Effects: Casts "msg" to a Mycommit_rep&
	
};	//<<	end Mycommit	>>
	
inline Mycommit_rep& Mycommit::rep() const { 

	th_assert(ALIGNED(msg), "Improperly aligned pointer");
	return *((Mycommit_rep*)msg); 
	
}
	
	
inline View Mycommit::view() const { return rep().v; }

inline Request_id Mycommit::request_id() const { return rep().rid; }

inline int Mycommit::rep_id() const { return rep().rep_id; }

inline int  Mycommit::bsize() { return rep().b_size;} 

inline Seqno Mycommit::seqno() const { return rep().seq_num; }

inline int Mycommit::id() const { return rep().rep_id; }

inline Digest& Mycommit::request_history_digest() const { return rep().rh_digest; }


/*inline char* Mycommit::mycommit(int &len) { 
  len = rep().mycommit_size;
  return contents()+sizeof(Mycommit_rep);
}*/	
inline bool Mycommit::full() const { return true; }	


inline bool Mycommit::match(Mycommit *myc) {

		return ( (request_history_digest() == myc->request_history_digest())&& 
        (rep().v == myc->rep().v) &&
        (rep().rid == myc->rep().rid) && 
		(rep().seq_num == myc->rep().seq_num)&&
		rep().b_size == myc->rep().b_size); 

}



/*inline bool Mycommit::check_mylocally_committed() {
  return (rep().last_committed >= rep().seq_num);
}*/

#endif // _Reply_h
	
	
	
	
	

	
	
	
	
	
	
	
	
	
	
