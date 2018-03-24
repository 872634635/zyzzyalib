#ifndef _Mycommit_h
#define _Mycommit_h 1


#include <strings.h>
#include "th_assert.h"
#include "Message_tags.h"
#include "Digest.h"
#include "Mycommit.h"
#include "Node.h"
#include "Principal.h"

#include "Statistics.h"	
	
Mycommit::Mycommit(Mycommit_rep *myc) : Message(myc) {}

//构造mycommit函数  mycommit消息中包含requests 跟对应的OR消息所包含的是一致的。
Mycommit::Mycommit(View view, Seqno seq_num, int bsize, int replica, Digest &rh_d,  Principal *p) :
	 Message(Mycommit_tag, sizeof(Mycommit_rep)+MAC_size)
{
			 
    rep().v = view; 
    rep().seqno = seq_num;
	rep().b_size=bsize;
    rep().rep_id = replica;
    rep().rh_digest = rh_d;
	
	//INCR_OP(mycommit_auth);
    //START_CC(mycommit_auth_cycles);
    p->gen_mac_out(contents(), sizeof(Mycommit_rep), contents()+sizeof(Mycommit_rep));
    //STOP_CC(reply_auth_cycles);
    set_size(sizeof(Mycommit_rep)+MAC_size);
	 // Append batched requests information
	 
	/*char *nexte = contents()+sizeof(Order_request_rep);
	int cur = 0;
	int i = seq_num;
	Digest	reqd;
	bool	stat = rh_log.request_digest(i, &reqd);
	
	Request *	req	= NULL;
	

	if (stat)
	{
		
		for (Request *	req= rqueue.lookup(reqd); req != 0 && cur < bsize; req= rqueue.lookup(reqd))
		{
			req	= rqueue.lookup(reqd);
			int cid = req->client_id();
			Request_id rid = req->request_id();
			Digest &d = req->digest();

			memcpy(nexte,(char*)&cid,sizeof(int));
			nexte += sizeof(int);
			memcpy(nexte,(char *)&rid,sizeof(Request_id));
			nexte += sizeof(Request_id);
			memcpy(nexte,(char *)&d,sizeof(Digest)); 
			nexte += sizeof(Digest);
			cur++;
			i++;
			
		}
	
	int old_size =  sizeof(Order_request_rep)+
    bsize*(sizeof(int)+sizeof(Request_id)+sizeof(Digest))+nd_size;
  
	set_size(old_size);*/
}

void Mycommit::authenticate( ) //对应加密
{
   th_assert((unsigned) node->auth_size() < msize()-sizeof(Mycommit_rep),
	    "Insufficient memory");

  int old_size = sizeof(Mycommit_rep);
  
  #ifndef USE_PKEY 
  set_size(old_size + node->auth_size());
  // Use authenticator 
  node->gen_auth_out(contents(), old_size, contents()+old_size);//到底是该in 还是out
  #else 
    // Use signature
	set_size(old_size+node->sig_size());
    node->gen_signature(contents(), old_size, contents()+old_size);
  #endif 
   //trim(); 
}

void Mycommit::re_authenticate(Principal *p, bool mac)
{
   // TO DO : Re-authenticate a principal using the new key
  if (p) {
    int old_size = size();
    
    if (mac) {
      th_assert((unsigned) MAC_size < (unsigned )(msize()-old_size),
	    "Insufficient memory");
      p->gen_mac_out(contents(),sizeof(Mycommit),contents()+old_size);
      set_size(old_size+MAC_size);
    }
    else {
      th_assert((unsigned) node->auth_size() < (unsigned) (msize()-old_size),
		"Insufficient memory");
      node->gen_auth_out(contents(), sizeof(Mycommit), contents()+sizeof(Mycommit));
      set_size(old_size+node->auth_size()); 
    }
    trim();
  }
}

//感觉需要两个verify()函数 
bool Mycommit::verify() 
{
	
	if (!node->is_replica(id())) 
    return false;

	const int replica_id = node->id();//当前replica的id
	const int n_id = myc_id();//收到的mycommit消息对应的replica id
	const int old_size = sizeof(Mycommit_rep);
	Principal* p = node->i_to_p(replica_id);


	if (p != 0) 
	{
		// 1.0 First check is properly authenticated      
		if (replica_id != nid  && size()-old_size >= node->auth_size()) //有疑问
		{//size():nassage size;
			if (!node->verify_auth_out(nid, contents(), sizeof(Mycommit_rep),
					 contents()+old_size)) 
			{
				return false;
			}
		} 
		else 
		{
		  // Message is signed.
			if (size() - old_size >= p->sig_size()) 
			{
				if (p->verify_signature(contents(), sizeof(Mycommit_rep),
					contents()+old_size, true)) 
				{
					return false;
				}
		  }
		}
	}
	
	
  return true;
}


 bool Reply::convert(Message *m1, Mycommit *&m2) {
  if (!m1->has_tag(Mycommit_tag, sizeof(Mycommit_rep)))
    return false;
  
  m1->trim();
  m2 = (Mycommit*)m1;
  return true;
}

//Mycommit.cc结束


/*

//send mycommitted_locally消息
Certificate<MyCommitted_locally> *mycl_reps;// Certificate with mycommit-locally message 应该在replica.h文件里
Certificate<MyCommit> *myc_reps;

#include "Certificate.t"
template class Certificate<MyCommitted_locally>;

#include "Certificate_order.t"
template class Certificate_order<Mycommit>;


myc_reps = new Certificate<MyCommitted_locally>(2*f()+1,2*f()+1,3*f()+1);
mc_reps = new Certificate_order<Mycommit>(2*f()+1,2*f()+1,3*f()+1);

void Replica::send_mycommitted_locally(Mycommit * m, Digest & d)
{
			int 			id	= m->cid();
			Seqno			s	= m->seqno();
			Principal * 	p	= i_to_p(id);
			
			 bool complete = myc_reps->is_complete();
			if(complete)
			{
				if (max_committed_locally < s)
				{
					max_committed_locally = s;

					//fprintf(stderr,"Cur req: %llu last exec: %llu last_stable: %llu chkpt : %llu \n",
					//	s, last_executed,last_stable,last_stable+checkpoint_interval);
					if ((s <= last_executed) && (s == last_stable + checkpoint_interval))
					{
						send_checkpoint(last_stable + checkpoint_interval);
					}
				}
			}

			//fprintf(stderr,"Sending committed locally : %llu	to client : %d \n", s, m->id());
			MyCommitted_locally * cl = new MyCommitted_locally(view(), m->request_id(), node->id(), d, p);

			send(cl, id);
			delete cl;								// TO DO : cache this??
}






//处理mycommit消息应该也在replica文件中
void Replica::handle(Mycommit * m)
{
	int mcount = 0;

	//fprintf(stderr,"Received locally committed : %llu from client : %d \n", m->seqno(), m->id());
	if (in_wv(m) && has_new_view())
	{
		if (max_committed_locally < m->seqno())
		{
			Digest	d;
			bool	res = rh_log.comp_history_digest(m->seqno(), &d);

			if ((res) && (d == m->request_history_digest()) && m->verify(mcount))
			{
				//fprintf(stderr,"Locally committed count: %d \n", mcount);
				if ((mcount >= (node->commit_qs() - 1)))
				{
					send_mycommitted_locally(m, d);
				}
			}
		}
		else 
		{
			Digest	d;
			bool	res = rh_log.comp_history_digest(m->seqno(), &d);

					if ((res) && (d == m->request_history_digest()))
						{
							send_mycommitted_locally(m, d);
						}
					}
				}
			else if (!in_wv(m))
				{
				
				Digest			d;

				assert(rh_log.comp_history_digest(last_stable, &d));
				send_committed_locally(m, d);

				//fprintf(stderr,"=============== Slow request ================= \n");
				}
			else 
				{
				}

			delete m;
}	
	
	
	//

*/	