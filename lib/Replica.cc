//改

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>	
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "th_assert.h"
#include "Message_tags.h"
#include "ITimer.h"
#include "Request.h"
#include "Checkpoint.h"
#include "New_key.h"
#include "Status.h"
#include "View_change.h"
#include "View_change_ack.h"
#include "New_view.h"
#include "Principal.h"
#include "Reply.h"
#include "Order_request.h"
#include "Mycommit.h"//改
#include "Mycommitted_locally.h"//改
#include "Request_history.h"
#include "Request_map.h"
#include "types.h"

#include "Replica.h"

#include "Statistics.h"

#include "State_defs.h"

// Global replica object.
Replica *		replica;

// Force template instantiation
#include "Certificate.t"

template class Certificate <Mycommit>;//改
template class Certificate <Order_request>;
#include "Log.t"
template class Log < Certificate<Order_request> >;
#include "Set.t"
template class Set <Checkpoint>;


template <class T> void Replica::retransmit(T * m, Time & cur, 
	Time * tsent, Principal * p)
{
	//XXthere most be a bug in the way tsent is managed. Figure out
	// where and reinsert this protection against denial of service attacks.
	//if (diffTime(cur, *tsent) > 1000) {	
	if (1)
		{
		//if (p->is_stale(tsent)) {
		if (1)
			{
			// Authentication for this principal is stale in message -
			// re-authenticate.
			m->re_authenticate(p);//加密更新
			}

		//	  printf("RET: %s to %d \n", m->stag(), p->pid());
		// Retransmit message
		send(m, p->pid());

		*tsent				= cur;
		}
}


#ifndef NO_STATE_TRANSLATION
Replica::Replica(FILE * config_file, FILE * config_priv, int num_objs, 
	int(*get) (int, char * *), 
	void(*put) (int, int *, int *, char * *), 
	void(*shutdown_proc) (FILE * o), 
	void(*restart_proc) (FILE * i), 
	short port, int bsize): Node(config_file, config_priv, port), rqueue(), 
rh_log(max_out), olog(max_out), elog(max_out * 2, 0), sset(n()), 
replies(num_principals), requests(num_principals), 
state(this, num_objs, get, put, shutdown_proc, restart_proc), 
vi(node_id, 0)
{
#else

	Replica::Replica(FILE * config_file, FILE * config_priv, char * mem, int nbytes): Node(config_file, config_priv),
		 rqueue (), ro_rqueue(), 
	olog(max_out), elog(max_out * 2, 0), pflog(max_view_changes), sset(n()), rh_log(max_out)
	state(this, mem, nbytes), replies(mem, nbytes, num_principals), 
	vi(node_id, 0)
		{

#endif

		// Fail if node is not a replica.
		if (!is_replica(id()))
			th_fail("Node is not a replica");

		seqno				= 0;
		last_stable 		= 0;
		low_bound			= 0;

		max_request_ordered = 0;
		max_mycommitted_locally = 0;//改
		last_executed		= 0;
		last_mycommit		= 0;//改
		batch_size			= bsize;
		max_batch_size		= bsize;
		checkpoint_stable	= 0;

		last_status 		= 0;
        myc_complete = false;//改
	//	start_local_commit = false;//改
		hate_primary		= false;
		hate_primary_view	= 0;

		limbo				= false;
		has_nv_state		= true;

		// Read view change, status, and recovery timeouts from replica's portion
		// of "config_file"
		int 			vt, st, it;

		fscanf(config_file, "%d\n", &vt);
		fscanf(config_file, "%d\n", &st);
		fscanf(config_file, "%d\n", &it);

		// Create timers and randomize times to avoid collisions.
		srand48(getpid());

		//fprintf(stderr," Itimer : %d \n", it);  
		//fprintf(stderr," Stimer : %d \n", st);  
		//fprintf(stderr," Vtimer : %d \n", vt);  
		itimer				= new ITimer(it + lrand48() % 100, itimer_handler);
		vtimer				= new ITimer(vt + lrand48() % 100, vtimer_handler);
		stimer				= new ITimer(st + lrand48() % 100, stimer_handler);
		
		//start_commit_timer = new ITimer(atimeout, start_commit_handler);
		//这里要怎么样调用 因为atimeout是在文件client中 

		// TO DO : Recoveries. It is important for nodes to recover in the reverse order
		// of their node ids to avoid a view-change every recovery which would degrade
		// performance.
		exec_command		= 0;
		non_det_choices 	= 0;

		join_mcast_group();

		// Disable loopback 
		u_char			l	= 0;
		int 			error = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &l, sizeof(l));

		if (error < 0)
		{
			perror("unable to disable loopback");
			exit(1);
		}

		
		myc_reps->clear();//应该在replica初始化的时候....................................
					
		
#ifdef LARGE_SND_BUFF
		int 			snd_buf_size = 262144;

		error				= setsockopt(sock, SOL_SOCKET, SO_SNDBUF, 
			(char *) &snd_buf_size, sizeof(snd_buf_size));

		if (error < 0)
			{
			perror("unable to increase send buffer size");
			exit(1);
			}

#endif

#ifdef LARGE_RCV_BUFF
		int 			rcv_buf_size = 131072;

		error				= setsockopt(sock, SOL_SOCKET, SO_RCVBUF, 
			(char *) &rcv_buf_size, sizeof(rcv_buf_size));

		if (error < 0)
			{
			perror("unable to increase send buffer size");
			exit(1);
			}

#endif
		}

	void Replica::register_exec(int(*e) (Byz_req *, Byz_rep *, Byz_buffer *, int, bool))
		{
		exec_command		= e;
		}

#ifndef NO_STATE_TRANSLATION
	void Replica::register_nondet_choices(void(*n) (Seqno, Byz_buffer *), 
		int 		max_len, 
		bool(*check) (Byz_buffer *))
		{
		check_non_det		= check;

#else

		void Replica::register_nondet_choices(void(*n) (Seqno, Byz_buffer *), int max_len)
			{
#endif

			non_det_choices 	= n;
			max_nondet_choice_len = max_len;
			}

		Replica::~Replica()
			{
				 //delete start_commit_timer;//添加的
			}

		void Replica::recv()
			{

			// Compute session keys and send initial new-key message.  
			Node::send_new_key();

			// Compute digest of initial state and first checkpoint.
			// TO DO : Zyzzyva : File system (to do)
			// state.compute_full_digest();
			// Start status and authentication freshness timers
			stimer->start();//这些计时器的作用
			atimer->start();
			
			// TO DO : Allow recoveries
			fprintf(stderr, "Replica ready with batch_size : %d \n", batch_size);

			while (1)
				{

				Message *		m	= Node::recv();

				// TODO: This should probably be a jump table.
				switch (m->tag())
					{
					case Request_tag:
						gen_handle <Request> (m);//调用的是handle（）函数
						break;

					case Order_request_tag:
						gen_handle <Order_request> (m);
						break;

					case Mycommit_tag://改
						gen_handle <Mycommit> (m);//是否也要有一个handle(Mycommit)函数
						break;

					case Checkpoint_tag:
						gen_handle <Checkpoint> (m);
						break;

					case New_key_tag:
						gen_handle <New_key> (m);
						break;

					case Hate_primary_tag:
						gen_handle <Hate_primary> (m);
						break;

					case Status_tag:
						gen_handle <Status> (m);
						break;

					case View_change_tag:
						gen_handle <View_change> (m);
						break;

					case View_change_ack_tag:
						gen_handle <View_change_ack> (m);
						break;

					case New_view_tag:
						gen_handle <New_view> (m);
						break;

					default:
						// Unknown message type.
						delete m;
					}
				}
			}


		void Replica::handle(Request * m)
			{
			int 			cid = m->client_id();
			bool			ro	= m->is_read_only();
			Request_id		rid = m->request_id();

			//fprintf(stderr," Received request from client %d rid ;%qu ro:%d  \n", 
			//cid,rid,ro);
			if (has_new_view() && m->verify())
				{
				// Replica's requests must be signed and cannot be read-only.  
				if (!is_replica(cid) || (m->is_signed() & !ro))
					{
					if (ro)
						{
						// Read-only requests.
						//fprintf(stderr,"received reqd only request from cid : %d rid :%llu\n",
						//	m->client_id(),m->request_id());
						execute_read_only(m);
						delete m;
						return;
						}

					// Last request that was (speculatively) executed from this client
					Request_id		last_rid = replies.req_id(cid);

					if (last_rid < rid)
						{
						//fprintf(stderr," Request accepted from client %d rid ;%qu ro:%d  \n", 
						//			   cid,rid,ro);
						// Request has not been executed.	
						if (rqueue.add_request(m))//rqueue是一个map (key,value)
							{
							//fprintf(stderr," Request added from client %d rid ;%qu ro:%d	\n", 
							//		   cid,rid,ro);
							if (id() == primary())
								{
								// Order the request and send it to other replicas
								// TO DO: Poll the socket for client requests, read all requests and then 
								// call send_order_request to maximize batching
								if (m->size() < Request::big_req_thresh)
									{
									// Relay the request to other replicas 
									send(m, All_replicas);
									}

								send_order_request(false);

								}
							}
						else 
							{
							// Request is already present
							if (!rqueue.is_ordered(m) && (id() != primary()))
								{
								// Relay the request to the primary if the request is not 
								// yet ordered
								send(m, primary());

								// Start per message I-HATE-THE-PRIMARY timer
								// If I receive f+1 message start the view change message
								// and stop responding to the messages 
								if ((rqueue.pending() >= batch_size) && (wait_cid != -1))
									{
									wait_cid			= m->client_id();
									wait_rid			= m->request_id();
									itimer->start();//I hate the primary

									//fprintf(stderr,"Starting timer: pending :%d batch_size : %d \n", 
									//		  rqueue.pending(), batch_size);
									}
								}

							if (!rqueue.is_ordered(m) && id() == primary())
								{
								// Order the request and send it to other replicas
								// batch size is set to current rqueue size
								// TO DO: Currently we reset the batch size to Max batch size
								// every check point interval. This does not adapt well when 
								// batch sizes increase faster than checkpoint interval. 
								batch_size			= rqueue.pending();
								send_order_request(true);
								}
							}
						}
					else if (last_rid == rid)
						{
						// If backup, retransmit response
						Reply * 		rep = replies.reply(cid);

						send(rep, cid);
						delete m;
						}
					else 
						{
						delete m;
						}
					}
				else 
					{
					delete m;
					}
				}
			else 
				{
				delete m;
				}
			}

		void Replica::send_order_request(bool retransmitted)
			{

			th_assert(primary() == node_id, "Non-primary called send_pre_prepare");

			// If there are requests in the request queue
			//fprintf(stderr," Max out :%d last stable : %llu seq no: %llu batch size : %d \n",
			//	max_out, last_stable, seqno, batch_size); 
			if (((rqueue.pending() >= batch_size) || retransmitted) && seqno + batch_size <= max_out + last_stable &&
				 has_new_view ())
				{

				// Adaptively set the batch size 
				// Create new order request message with the next sequence number
				Order_request * oreq = new Order_request(view(), ++seqno, rqueue, rh_log, requests);

				// Authenticate the order request and multicast the order request to other replicas 
				oreq->authenticate();
				send(oreq, All_replicas);			//???????????????????????

				// Update the sequence number to match the max seq number used in this batch
				int 			bsize = oreq->bsize(); // batch size

				seqno				+= (bsize - 1);

				if (max_request_ordered < seqno)
					{
					max_request_ordered = seqno;
					}

				// Add the order request message to the log // oc 和olog有点迷糊
				Certificate <Order_request> &oc = olog.fetch(seqno);//什么时间olog更新的
				th_assert(oc.add(oreq), "failed adding request \n"); // Add message to the log for retransmissions

				// Execute the request speculatively
				// fprintf(stderr,"Sent order request %llu to other replicas \n", seqno);
				if (!oreq->commit_opt())
					{
					// Execute the request immediately if commit opt is not on
					// fprintf(stderr,"NO commit opt: Can execute request speculatively \n");
					execute_request_speculatively(seqno);
					}
				}
			}


		template <class T> bool Replica::in_w(T * m)
			{
			const Seqno 	offset = m->seqno() -last_stable;

			if (offset > 0 && offset <= max_out)
				return true;

			if (offset > max_out && m->verify())
				{
				// Send status message to obtain missing messages. This works as a
				// negative ack.
				//send_status();
				}

			return false;
			}


		template <class T> bool Replica::in_wv(T * m)
			{
			const Seqno 	offset = m->seqno() -last_stable;

			/*if (m->seqno() > 512) {
			  fprintf(stderr,"offset : %lld max_out : %d my view : %lld msg view: %lld \n",
				  offset, max_out,	m->view(),	view());
				  }*/
			if (offset > 0 && offset <= max_out && m->view() == view())
				return true;

			if ((m->view() > view() || offset > max_out) && m->verify())
				{
				// Send status message to obtain missing messages. This works as a
				// negative ack.
				//send_status();
				}

			return false;
			}

		void Replica::compute_non_det(Seqno s, char * b, int * b_len)
			{
			if (non_det_choices == 0)
				{
				*b_len				= 0;
				return;
				}

			Byz_buffer		buf;

			buf.contents		= b;
			buf.size			= *b_len;
			non_det_choices(s, &buf);
			*b_len				= buf.size;
			}


void Replica::handle(Order_request * m)
{
	
	Seqno ms = m->seqno();
	int bsize = m->bsize();
				

	// Now read the non_deterministic values and verify
	Byz_buffer b;
	b.contents = m->choices(b.size);

	//fprintf(stderr,"BACKUP:  ORDERED REQUEST RECV : %lld bsize : %d \n", ms, bsize); 
	// process the message if valid 
	//fprintf(stderr," Max out :%d last stable : %llu seq no: %llu batch size : %d \n",
	//	max_out, last_stable, ms, bsize); 
	if (in_wv(m) && ms > low_bound && has_new_view())
	{
		Certificate <Order_request> &oc = olog.fetch(ms + bsize - 1);//返回对应的记录，只返回最后一个请求的记录吗？？？

		//fprintf(stderr,"BACKUP:  ORDERED REQUEST ACCEPTED : %lld \n", ms); 
		// Only accept order-request message m if we never accepted
		// a message with sequence number ms, message m is correct
		if (oc.add(m))//将m加入oc该certificate中，如果m满足一定的条件
		{
			if ((m->id() == primary()) && (id() != primary()))//m->id()：send该m的replica的id id()返回当前node的id
			{ 
				//fprintf(stderr," Inside addition: Added OR from %d \n", m->id());
				// Authenticate the OR message
				Order_request * oreq;
				assert(oc.read_rep(oreq, primary()));//从oc中读从primary收到的信息
				oreq->my_auth();			// Replace primary's authenticator with mine

				// Use this authenticated message to relay the 
				// the order request message to other replicas (to commit ?????????????????????
				// the request) or client (in the reply message).
				oc.add_mine(oreq);			// Add my order request message after adding primary's
				//如果值对，将oreq加入certificate，这时该replica成为该oreq的owner

				View 	v = m->view();
				Digest 	rh_d;
				Digest 	rh_d2 = m->request_history_digest();//改。。。。
				//int 	myid = id();//改。。。。
				Seqno 	ls = rh_log.last_complete(); // last ordered request with no holes
				Seqno 	i = ms;
					
				//fprintf(stderr,"Added OR request from client : %d for :%llu\n",
				// m->cid(ms),ms);
				// Add the request to the history log and cache
				for (i = ms; i < ms + bsize && m->is_present(i); i++)
				{

					Digest	d;

					memcpy((void *) &d, (void *) m->digest(i), sizeof(Digest));
					int *			cid = m->cid(i);
					Request_id *	rid = m->rid(i);

					// Reset the I_HATE_PRIMARY timer if we recieve the order request 
					if ((wait_cid == *cid) && (wait_rid = *rid))
					{
						wait_cid = -1;
						itimer->stop();//定时器 注意什么时间开启的

						//fprintf(stderr,"Stoping timer: pending :%d batch_size : %d \n", 
						//		  rqueue.pending(), batch_size);
					}

					// fprintf(stderr,"Received request cid ; %d rid : %lld \n", *cid,*rid);
					// Set order for the request
					rqueue.order_request(NULL, *cid, *rid, d, v, i);//如果没有 在map中为其创建一条记录

					// TO DO : reset the I-HATE-THE-PRIMARY timer as soon as the 
					// order is received
					// Add the entry in the request history log
					rh_log.add_request(i, *cid, *rid, d, b.contents, b.size, ms + bsize - 1);//加入request history log

					// Add request to the cache
					requests.add(*cid, *rid, m);

				}//end for//所有这个batch的requests都add???????

				// TO DO : Start a view change or send status message to other replicas to fill holes
				// if all requests are not included in this batch
				th_assert(i == ms + bsize, "Primary did not include all requests in the batch. \n");


				// Find the maximum stable request that has no holes (missing entry in the ordered 
				// request history log) before it in the request history log 
				Seqno	cs	= rh_log.update_history_digest();

				if (cs > ls)
				{
					// There are some holes but there are stable requests that are 
					// not executed
					Seqno	i = ls + 1;//从该batch的第一个请求开始

					while (i <= cs)
					{

						if (max_request_ordered < i)
						{
							max_request_ordered = i;
						}

						// Send the order request or execute request 
						Seqno	bnum = rh_log.batch_snum(i);

						Certificate <Order_request> &ocert = olog.fetch(bnum);
						th_assert(ocert.read_rep(oreq, id()), "Request is not ordered by primary");

						// Send order request if commit optimization is set by a client 
						// or if it is time to checkpoint
						if (oreq->commit_opt() || time_to_checkpoint(i, bnum, last_stable))
						{//commit_opt()首先为false 然后看是否到时间checkpoint
							if (oreq->commit_opt())//XXXXXXXXXXXXXXXXXXX
							{
								//fprintf(stderr,"Commit opt is on for seq: %llu broadcast \n", bnum);
							}

							//fprintf(stderr,"Sending Commit i:%llu  bnum: %llu last_stable: %llu \n", i,bnum,last_stable);
							send(oreq, All_replicas); //如果要checkpiont将该OR发个所有副本 ???????????
						}

						if (!oreq->commit_opt())
						{
							// Execute the request immediately if commit opt is not on
							fprintf(stderr,"NO commit opt: Can execute request speculatively \n");
							execute_request_speculatively(bnum);//bnum是batch中的最大的序列号，执行完所有请求后执行并回复给client
							//。。。执行过之后发送mycommit消息
							//start_commit_timer->start(); 先不管该计时器
							send_mycommit( m, rh_d2);
							
							
									
						}

						if (ocert.is_complete() && (max_mycommitted_locally < bnum))
						{//为什么这里要看OC certificate是否complete 
							//max_committed_locally = bnum;
							max_mycommitted_locally = bnum;//改
							// execute the request after it is locally committed
							if (oreq->commit_opt())//XXXXXXXXXXXXXXX
							{
								//fprintf(stderr,"Commit opt is on: Execute Speculatively : %llu committed cur : %llu le : %llu\n", 
								//	 bnum, max_committed_locally, last_executed);
								execute_request_speculatively(max_mycommitted_locally);
	
							}

							// Start checkpoint when it is time 
							if (time_to_checkpoint(i, bnum, last_stable))
							{
								send_checkpoint(last_stable + checkpoint_interval);
							}
						}

						i	= bnum + 1;
					}

							// Check to see if cs matches with ms+bsize-1. Otherwise,
							// send status message now or wait until I make no progress??
							// For now, avoid extra work (by not sending the message) 
							// until the point I make no progress
				}
				else 
				{
					// There are holes and I cannot make any progress
					// Send status message to fill the holes
					th_assert(cs == ls, "Failed to execute request \n");
					Status st(view(), cs + 1, GET_HOLE_TYPE);

					send_status(&st);
				}
			}

			else 
			{
				// OR request message is received from other replica
				// 1.0 Check if we can commit the request (at least 2f other replicas have the same order)
				if (oc.is_complete() && (max_request_ordered >= ms))//该执行执行 并更新相关变量
				{

					// 1.1 If there are no holes then commit the message and speculatively execute requests 
					Seqno	bnum = rh_log.batch_snum(ms);

					if (max_mycommitted_locally < bnum)
					{
						max_mycommitted_locally = bnum;

						// Commit optimization: Execute the request after it is committed
						if (last_executed < max_mycommitted_locally)
						{
							//fprintf(stderr,"Cert complete: Execute Speculatively : %llu committed cur : %llu le : %llu\n", 
							//bnum, max_committed_locally, last_executed);
							execute_request_speculatively(max_mycommitted_locally);
						}
					}

					// 1.2 Send checkpoint message if it is time for garbage collection
					Order_request * oreq = oc.cvalue();

					th_assert(oreq, "Missing c value \n");
					int bsize = oreq->bsize(); // batch size of this request

					//fprintf(stderr,"OR: Committed req: %lld bnum : %lld last_stable: %lld bsize : %d \n", ms,bnum,last_stable,bsize);
					if (time_to_checkpoint(bnum - bsize + 1, bnum, last_stable))
					{
						/* (bnum-bsize+1 <= (last_stable + checkpoint_interval)) && 
						(bnum >= (last_stable + checkpoint_interval))) { */
						send_checkpoint(last_stable + checkpoint_interval);
					}

				}
				else 
				{
					if (max_request_ordered < ms)
					{
						// There are holes and I cannot make any progress
						// Send status message to fill the holes
						//fprintf(stderr,"===HOLES === HOLES === \n");
						Status st(view(), max_request_ordered + 1, GET_HOLE_TYPE);

						send_status(&st);
					}
				}
			}
		}
	}
	else 
	{
		//fprintf(stderr," About to delete OR %x from %d \n", m, m->id());
		delete m;
	}

	return;
}


		void Replica::execute_request_speculatively(Seqno s)
			{

			//fprintf(stderr, "execute speculatively :%lld \n", s);
			if (last_executed < s)
				{
				for (int i = last_executed + 1; i <= s; i++)
					{
					// fprintf(stderr, "execute speculatively cur :%d \n", i);
					Seqno			bnum = rh_log.batch_snum(i);

					Certificate <Order_request> &oc = olog.fetch(bnum);
					Order_request * oreq;

					th_assert(oc.read_rep(oreq, id()), "Request is not ordered by primary");

					if (oreq->commit_opt() && (max_mycommitted_locally < bnum))
						{
						// If commit opt. then the request is not executed until 
						// it is committed
						// fprintf(stderr,"Commit opt is on: cannot execute until %llu is committed cur : %llu\n", bnum, max_committed_locally);
						break;
						}
					else 
						{

						// Execute the request
						// 1.0 Check if request history digest is computed and 
						//	   full request is present
						Digest			reqd;
						bool			stat = rh_log.request_digest(i, &reqd);
						Request *		r	= NULL;

						if (stat)
							{
							r					= rqueue.lookup(reqd);

							// 2.0 TO DO : Spawn the execution thread for concurrent requests
							//	   for now we use signgle thread.
							if (r)
								{
								//fprintf(stderr,"executing request cid : %d \n", r->client_id());
								// Execute the request if there is a valid request
								execute_request(i, r);

								// 3.0 Update last executed
								last_executed		= i;

								// Garbage collect at checkpoint interval 
								// if we received 2f+1 checkpoint messages before executing the request
								if ((i == last_stable + checkpoint_interval))
									{
									exec_checkpoint(i);

									if (checkpoint_stable >= i)
										{
										// This can happen when request is executed after checkpoint is 
										// marked stable when the replica uses commit optimization.
										mark_stable(i);
										}
									}
								}
							else 
								{
								// Cannot execute this request as I do not have 
								// the request although I have the order 
								// Fetch this request from other replicas
								Status st(view(), i, reqd, GET_REQUEST_TYPE);

								send_status(&st);
								break;
								}
							}
						}
					}
				}
			}

		void Replica::exec_checkpoint(Seqno c)
			{
			// TO DO: Upcall to the application to take a checkpoint
			}

		void Replica::exec_checkpoint_stable(Seqno c)
			{
			// TO DO: Upcall to the application to make the checkpoint stable
			}

		void Replica::exec_rollback(Seqno c)
			{
			// TO DO: Upcall to the application to 
			//		  rollback the execution state to checkpoint with seqno c
			}

		void Replica::exec_rollforward(Seqno c)
			{
			// TO DO: Upcall to the application to rollforward the execution state to c
			// This happens when this replica is far left behind compared to other replicas
			// and other replicas have garbage collected the request information.
			}

		void Replica::add_or_msg(Seqno s, Reply * rep)
			{
			// Include order_request message
			// We use the same batched order_request message for all requests in the batch
			// Include order_request message
			// We use the same batched order_request message for all requests in the batch
			int 			bsnum = rh_log.batch_snum(s);

			Certificate <Order_request> &oc = olog.fetch(bsnum);
			Order_request * oreq = NULL;

			assert(oc.read_rep(oreq, id()));		// Read the order request message 
			rep->authenticate_copy_order(oreq); 	// Authenticate and copy the order request message in the reply 	

			}

		void Replica::execute_request(Seqno s, Request * req)
			{
			Digest			rh_d;

			// TO DO: Multithreaded execution: Enqueue requests and execute them later
			if (rh_log.comp_history_digest(s, &rh_d))
				{

				Reply * 		rep = new Reply(view(), req->request_id(), s, max_mycommitted_locally, rh_d, node_id);

				add_or_msg(s, rep); 				// Add Order_request message before authenticating the reply 

				if (is_exec_replica(id()))
					{
					// Obtain "in" and "out" buffers to call exec_command
					Byz_req 		inb;
					Byz_rep 		outb;

					inb.contents		= req->command(inb.size);
					outb.contents		= rep->store_reply(outb.size);

					// Execute command.
					int 			cid = req->client_id();
					Request_id		rid = req->request_id();


					Principal * 	cp	= i_to_p(cid);
					int 			error = exec_command(&inb, &outb, 0, cid, true);

					if (!error)
						{
						// Finish constructing the reply and send it.
						if (outb.size < 50 || req->replier() == node_id || req->replier() < 0)
							{
							rep->authenticate(cp, outb.size);

							// Send full reply.
							if (replies.add(cid, rep))
								{
								//fprintf(stderr, "EXE(WHOLE): Sending response : %lld from client %d rid :%lld \n", s, cid, rid);
								send(rep, cid);
								}
							else 
								{
								delete rep;
								}
							}
						else 
							{
							// Send empty reply.
							Digest			rd;

							rep->comp_digest(rd, outb.size);
							Reply empty(view(), rid, s, max_mycommitted_locally, node_id, rh_d, rd, cp);

							add_or_msg(s, &empty);
							empty.re_authenticate(cp);

							//fprintf(stderr, "EXEC (Digest): Sending response : %lld from client %d rid :%lld \n", s, cid, rid);
							send(&empty, cid);
							delete rep;
							}
						}
					}
				else 
					{
					// Not an execution replica, then just send the request history digest
					int 			cid = req->client_id();
					Principal * 	cp	= i_to_p(cid);

					add_or_msg(s, rep); 			// Add OR request message before authenticating the reply
					rep->re_authenticate(cp);

					if (replies.add(cid, rep))
						{
						//fprintf(stderr, "NON_EXEC: Sending response : %lld from client %d rid :%lld \n", s, cid, req->request_id());
						send(rep, cid);
						//send(rep, All_replicas);
						}
					else 
						{
						delete rep;
						}
					}
				}
			}

		void Replica::execute_read_only(Request * req)
			{
			Digest			rh_d;
			Seqno			s	= rh_log.update_history_digest();

			// TO DO : Enqueue requests and execute them later
			if (rh_log.comp_history_digest(s, &rh_d))
				{

				Reply * 		rep = new Reply(view(), req->request_id(), s, max_mycommitted_locally, rh_d, node_id);

				if (is_exec_replica(id()))
					{
					// Obtain "in" and "out" buffers to call exec_command
					// Obtain "in" and "out" buffers to call exec_command
					Byz_req 		inb;
					Byz_rep 		outb;

					inb.contents		= req->command(inb.size);
					outb.contents		= rep->store_reply(outb.size);

					// Execute command.
					int 			cid = req->client_id();
					Request_id		rid = req->request_id();
					Principal * 	cp	= i_to_p(cid);
					int 			error = exec_command(&inb, &outb, 0, cid, true);

					if (!error)
						{
						// Finish constructing the reply and send it.
						rep->authenticate(cp, outb.size);

						if (outb.size < 50 || req->replier() == node_id || req->replier() < 0)
							{
							// Send full reply.
							send(rep, cid);
							delete rep;
							}
						else 
							{
							// Send empty reply with digest
							Digest			rd;

							rep->comp_digest(rd, outb.size);
							Reply empty(view(), rid, s, max_mycommitted_locally, node_id, rh_d, rd, cp);

							empty.re_authenticate(cp);
							send(&empty, cid);
							delete rep;
							}
						}
					}
				else 
					{
					// Not an execution replica, then just send the request history digest
					int 			cid = req->client_id();
					Principal * 	cp	= i_to_p(cid);

					rep->re_authenticate(cp);

					// read only response should not be cached
					if (rep != NULL)
						{
						//fprintf(stderr,"Sending response to cid : %d rid :%llu \n",
						//		 cid,req->request_id());
						send(rep, cid);
						delete rep;
						}
					}
				}
			}

		void Replica::handle(New_key * m)
			{

			int 			id	= m->id();
			Principal * 	p	= i_to_p(id);

			if (!p->check_key() && id < num_replicas)
				{
				send(last_new_key, id);
				}

			//fprintf(stderr,"Recived new key from %d \n", id);  
			if (!m->verify())
				{
				//printf("BAD NKEY from %d\n", m->id());
				}

			delete m;
			}

		// Send committed locally message to other replicas 
		// as a checkpoint message when 2f+1 matching locally ordered 
		// messages are recieved 
		void Replica::send_checkpoint(Seqno c)
		{
			//fprintf(stderr, "Send Checkpoint: %lld last executed: %lld max locally committed: %lld \n",c,last_executed,max_committed_locally);
			if (c > max_mycommitted_locally)
				{
				// You cannot garbage collect unless it is committed
				return;
				}

			// Send the order request  ****//send the checkpoint message
			Seqno			bnum = rh_log.batch_snum(c);

			Certificate <Order_request> &oc = olog.fetch(bnum);
			Order_request * oreq = oc.cvalue();

			th_assert(oreq, "Request is not committed locally");
			Checkpoint *	ck	= new Checkpoint(c, oreq->request_history_digest());

			send(ck, All_replicas); 				//send the checkpoint message
			Certificate <Checkpoint> &cs = elog.fetch(c);

			if (cs.add(ck) && cs.is_complete())
			{
				//fprintf(stderr, "Checkpoint complete : %lld last executed: %lld \n", c,last_executed);
				checkpoint_stable	= c;

				// Make sure that enough requests executed
				if (last_executed >= c)
				{
				mark_stable(c);
				}
			}

		}

		// Send committed locally to the client in the second phase
		// in response to local commit message
		/*void Replica::send_committed_locally(Local_commit * m, Digest & d)
		{
			int 			id	= m->id();
			Seqno			s	= m->seqno();
			Principal * 	p	= i_to_p(id);

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

			//fprintf(stderr,"Sending committed locally : %llu	to client : %d \n", s, m->id());
			Committed_locally * cl = new Committed_locally(view(), m->request_id(), node->id(), d, p);

			send(cl, id);
			delete cl;								// TO DO : cache this??
		}*/

		//新加函数 send_mycommitted_locally
		// 当收到2f+1个match的其他副本发来的mycommit消息之后给client发消息
		
		void Replica::send_mycommitted_locally(Mycommit * m, Digest & d)//这里的d是请求历史摘要
		{
			//只需要给对应的mycommit消息发mycommitted_locally消息
			int Id = id();//当前节点(replica)的id
			Seqno s	= m->seqno();//primary发来的OR消息相应的序列号
			Principal * p = i_to_p(Id);
			bool complete = myc_reps->is_complete();//myc_reps什么时间归零 更新
		
			Seqno	bnum = rh_log.batch_snum(s);
			
			if (complete )
			{
				for( int i = s; i<= bnum;i++)//注意循环的条件
				{
					//batch_snum Max sequence number of the batch that this request is part of
					Digest	reqd;
					bool	stat = rh_log.request_digest(i, &reqd);
					Digest d ;
					Request *	r	= NULL;
					if (stat)
					{
						r	= rqueue.lookup(reqd);
						
						if (r)
						{
							Seqno rcid = r->client_id() ;
							Principal * p = i_to_p(rcid);
							
							rh_log.comp_history_digest(i, &d);//注意这里的digest d
							
							if( max_mycommitted_locally < s )
							{
								Mycommitted_locally * mycl = new Mycommitted_locally(view(), r->request_id(), node->id(), d, p);
								send(mycl, rcid);
								max_mycommitted_locally = i;
								delete mycl;
							}
							else 
							{
								break;
							}
						}
					}
				}
			}
		}
		
		//注意verify
		//根据上下文 还是改成handle(Mycommit*)
		void Replica::handle(Mycommit * m)
		{
			Seqno			ms	= m->seqno();
			int 			bsize = m->bsize();
			
			Seqno mcl = max_mycommitted_locally;

			
			if ( !in_wv(m)|| !has_new_view()|| !m->verify() ) 
			{
				delete m;
				//fprintf(stderr,"Failed to read response with rid :%llu cur rid :%llu cid : %d \n", 
				//rep->request_id(),out_rid,rep->id());
							
				if (ms < mcl)
				{
					if(!myc_reps->add(m)) {
					//fprintf(stderr,"Failed to add response with rid :%llu cid : %d Count : %d recvd : %d\n", 
					//   rep->request_id(),rep->id(),r_reps->num_correct(), r_reps->num_elements() );
					}
					else {
							//fprintf(stderr,"Added response with rid :%llu cid : %d Count : %d recvd : %d\n", 
							//rep->request_id(),rep->id(),r_reps->num_correct(), r_reps->num_elements());
					}

					if ((myc_reps->max_match()) ||(myc_reps->is_complete() )) 
					{
						Digest			d;
						d = m->request_history_digest();
						send_mycommitted_locally(m,d);
				
					}   
				}
			}
					
		}
			
		/////
		void Replica::send_mycommit(Order_request * m, Digest & d)
		{
			Seqno ms = m->seqno();
			int bsize = m->bsize();
			View 	v = m->view();
			int Id = id();
			Principal * p = i_to_p(Id);
			
			if(last_mycommit < ms)
			{
				Mycommit *myc =new Mycommit(v,ms,bsize,id(),d,p);
				myc->authenticate();
				send(myc,All_replicas);//改。。发mycommit消息给所有副本
						
				//mycommits.add(*rid, myc);//cache跟requests一样 看是怎么定义的
				last_mycommit = ms+bsize-1;//replica需加入该变量
				//感觉只需要把最后发出的mycommit对应的序列号给出就行了
			}
										
		}

		
		/*void Replica::handle(Local_commit * m)
		{
			int 			mcount = 0;

			//fprintf(stderr,"Received locally committed : %llu from client : %d \n", m->seqno(), m->id());
			if (in_wv(m) && has_new_view())
				{
				if (max_committed_locally < m->seqno())
					{
					Digest			d;
					bool			res = rh_log.comp_history_digest(m->seqno(), &d);

					if ((res) && (d == m->request_history_digest()) && m->verify(mcount))
						{
						//fprintf(stderr,"Locally committed count: %d \n", mcount);
						if ((mcount >= (node->commit_qs() - 1)))
							{
							send_committed_locally(m, d);
							}
						}
					}
				else 
					{
					Digest			d;
					bool			res = rh_log.comp_history_digest(m->seqno(), &d);

					if ((res) && (d == m->request_history_digest()))
						{
						send_committed_locally(m, d);
						}
					}
				}
			else if (!in_wv(m))
				{
				// The request history digest for this request is garbage collected
				// Send the request history digest of the lowest sequence number 
				// in this current window
				// ===================== IMPORTANT : TO DO ==============================
				// We should infact cache the request history digest per client
				// along with the reply sent to it and use it here instead of last_stable digest to 
				// send the committed locally response 
				Digest			d;

				assert(rh_log.comp_history_digest(last_stable, &d));
				send_committed_locally(m, d);

				//fprintf(stderr,"=============== Slow request ================= \n");
				}
			else 
				{
				}

			delete m;
			}*/

		void Replica::handle(Checkpoint * m)
			{
			const Seqno 	ms	= m->seqno();

			// Only accept messages with the current view.	TODO: change to
			// accept commits from older views as in proof.
			//fprintf(stderr, "HANDLE: Checkpoint complete : %lld last executed: %lld committed locally : %lld \n", ms,last_executed,max_committed_locally);
			if (in_w(m) && ms > low_bound)
				{
				Certificate <Checkpoint> &cs = elog.fetch(m->seqno());

				if (cs.add(m) && cs.is_complete() && m->seqno() <= max_mycommitted_locally)
					{
					//fprintf(stderr, "Checkpoint successful: %lld last executed: %lld \n", ms,last_executed);
					// Make sure that enough requests executed
					if (last_executed >= ms)
						{
						checkpoint_stable	= ms;
						Seqno			bnum = rh_log.batch_snum(m->seqno());

						Certificate <Order_request> &oc = olog.fetch(bnum);
						Order_request * oreq;

						th_assert(oc.read_rep(oreq, id()), "Failed reading oreq \n");

						if (m->digest() == oreq->request_history_digest())
							{
							mark_stable(ms);
							}
						}
					}

				return;
				}

			delete m;
			return;
			}


		void Replica::handle(Status * m)
			{
			//fprintf(stderr, "Received  status %d %lld %lld from replica :%d \n", m->type(), 
			//m->snum(),last_stable,m->id());
			if (m->verify())
				{
				Seqno			ls	= m->snum();
				short			type = m->type();

				if (type == GET_HOLE_TYPE)
					{
					// Missing order request
					//fprintf(stderr,"HOLE	\n");
					if (ls > last_stable)
						{
						Certificate <Order_request> &oc = olog.fetch(ls);
						Order_request * oreq;

						// fprintf(stderr,"Hmm.. garbage collected : %lld last_stable :%lld \n", ls, last_stable);
						// Send it if I have it
						if (oc.read(oreq, 0) && oreq)
							{
							// fprintf(stderr,"Sent order request id : %d size : %d \n", oreq->id(),oreq->size());
							send(oreq, m->id());
							}
						else 
							{
							// Else forward it to the primary it should have it
							if (id() != primary())
								send(m, primary());
							}
						}
					else 
						{
						// fprintf(stderr,"Hmm.. garbage collected : %lld last_stable :%lld \n", ls, last_stable);
						}
					}
				else if (type == GET_REQUEST_TYPE)
					{
					// Missing request
					// TO DO : return the request from the cache
					// for now but we should check in the request_map also
					Request *		r	= rqueue.lookup(m->digest());

					// fprintf(stderr,"MISSING REQ \n");
					if (r)
						{
						// Send it if I have it
						// fprintf(stderr,"Sent request rid : %llu cid : %d \n", r->request_id(), r->client_id());
						send(r, m->id());
						}
					else 
						{
						// Else forward it to the primary it should have it
						// This can slow down the primary
						// if (id() != primary()) send(m,primary());
						}
					}
				}
			else 
				{
				fprintf(stderr, "Failed status %d %lld %lld from replica :%d \n", m->type(), 
				m->snum(), last_stable, m->id());
				}

			delete m;
			}


		// Collect view change info. from 2f+1 replicas
		// Then reconcile the tentative state from these replicas
		void Replica::handle(View_change * m)
			{
			fprintf(stderr, "Received a view change from: %d with view: %llu cur : %llu \n", 
			m->id(), m->view(), v);

			if (m->id() == primary() && m->view() > v)
				{
				if (m->verify())
					{
					// "m" was sent by the primary for v and has a view number
					// higher than v: move to the next view
					send_hate_primary();
					}
				}

			bool			modified = vi.add(m);

			if (!modified)
				return;

			// TODO: memoize maxv and avoid this computation if it cannot change i.e.
			// m->view() <= last maxv. This also holds for the next check.
			View			maxv = vi.max_view();

			if (maxv > v)
				{
				// Replica has at least f+1 view-changes with a view number
				// greater than or equal to maxv: change to view maxv.
				v					= maxv - 1;
				send_view_change();

				return;
				}

			if (limbo && primary() != node_id)
				{
				maxv				= vi.max_maj_view();
				th_assert(maxv <= v, "Invalid state");

				if (maxv == v)
					{
					// Replica now has at least 2f+1 view-change messages with view  greater than
					// or equal to "v"
					// Start timer to ensure we move to another view if we do not
					// receive the new-view message for "v".
					vtimer->start();
					limbo				= false;
					}
				}
			}


		// New primary sends the new view message
		void Replica::handle(New_view * m)
			{
			fprintf(stderr, "Received new view from id: %d \n", m->id());
			vi.add(m);
			}


		void Replica::handle(View_change_ack * m)
			{
			fprintf(stderr, "Received view change ack from id: %d \n", m->id());
			vi.add(m);
			}


		// Send view change with following information
		// latest checkpoint with certificate, latest locally committed request 
		// with certificate, all the ordered requests with history digests
		// since latest locally committed request 
		void Replica::send_view_change()
			{

			// reset hate primary variables
			hate_primary		= false;

			// Move to next view.
			v++;
			cur_primary 		= v % num_replicas;
			limbo				= true;
			vtimer->stop(); 						// stop timer if it is still running

			//ntimer->restop();
			// Add last stable checkpoint
			vi.add_chkpt_digest(chkpt_digest());

			// Add requests between last stable+1 and last executed
			for (Seqno cur = last_stable + 1; cur <= last_executed; cur++)
				{
				Seqno			bsnum = rh_log.batch_snum(cur);

				Certificate <Order_request> &ocert = olog.fetch(bsnum);
				Order_request * orequest = NULL;

				th_assert(ocert.read_rep(orequest, id()), "Failed reading oreq \n");

				// Read the order request message 
				vi.add_request(orequest, cur);
				}

			// Create and send view change message
			vi.view_change(v, last_stable, max_mycommitted_locally, last_executed);
			}

		void Replica::handle(Hate_primary * m)
			{
			View			v	= m->view();

			fprintf(stderr, "Received hate primary from : %d \n", m->id());

			if (v >= view())
				{
				// We store the view v in "v+1" location in the pflog
				Certificate <Hate_primary> &pfcert = pflog.fetch(v + 1);
				pfcert.add(m);

				if (pfcert.is_complete())
					{
					fprintf(stderr, "Sending view change \n");
					send_view_change();
					}
				}
			}


		void Replica::send_hate_primary()
			{
			View			v	= view();

			fprintf(stderr, "Sending hate primary : %lld \n", v);
			Hate_primary *	hp	= new Hate_primary(v);

			// We store the view v in "v+1" location in the pflog
			Certificate <Hate_primary> &pfcert = pflog.fetch(v + 1);
			pfcert.add(hp);
			send(hp, All_replicas);
			hate_primary		= true;
			hate_primary_view	= v;

			if (pfcert.is_complete())
				{
				fprintf(stderr, "Sending view change \n");
				send_view_change();
				}

			itimer->stop();
			}

		void Replica::process_new_view(Seqno min, Seqno last_committed, Seqno max)
			{
			vtimer->restop();
			limbo				= false;

			fprintf(stderr, " Process new view \n");

			if (primary(v) == id())
				{
				New_view *		nv	= vi.my_new_view();

				fprintf(stderr, "Send new view \n");
				send(nv, All_replicas);
				}

			// 1. Reconcile local last stable 
			//	 1.1 my_last_stable < last_stable and le > last_stable
			//		 -- if my_rhd(last_stable) matches with nv->ckpt(last_stable)
			//			   -- Set my_last_stable = last_stable
			//		 -- else 
			//			   -- Impossible: assert
			if ((last_stable < min))
				{
				mark_stable(last_stable);
				}


			//	1.2  max > my_last_stable > last_stable
			//		  -- if my_rhd(my_last_stable) == nv->rh_digest(my_last_stable)
			//			   -- Set min = my_last_stable
			//			   -- send checkpoint msg to other replicas with my_last_stable
			//		  -- else 
			//			   -- Impossible: assert
			if (last_stable > min)
				{
				min 				= last_stable;
				}

			low_bound			= min;

			// 2. If I am lagging behind fetch app state from other replicas 
			if (last_executed < min)
				{
				has_nv_state		= false;
				exec_rollforward(min);
				return;
				}

			// 2. Reconcile local executed requests
			th_assert(min >= last_stable && max - last_stable - 1 <= max_out, "Invalid state");

			for (Seqno i = last_stable + 1; i < max; i++)
				{

				if (i <= last_executed)
					{
					Digest			d;

					th_assert(vi.fetch_history_digest(i, d), "Invalid call \n");
					Seqno			bsnum = rh_log.batch_snum(i);

					Certificate <Order_request> &oc = olog.fetch(bsnum);
					Order_request * oreq;

					oc.read_rep(oreq, id());

					if (d != oreq->request_history_digest())
						{
						// TO DO: Rollback state to the last stable and read all requests
						exec_rollback(last_stable);
						last_executed		= last_stable;

						if (max_mycommitted_locally > last_stable)
							{
							max_mycommitted_locally = last_stable;
							}

						max_request_ordered = last_stable;
						break;
						}

					// Send oreq to commit 
					if (i == last_committed)
						{
						max_mycommitted_locally = last_committed;
						}

					if (i == last_stable + checkpoint_interval)
						{
						if (max_mycommitted_locally < i)
							{
							send(oreq, All_replicas);
							}
						}
					}
				}

			for (Seqno i = last_executed + 1; i < max; i++)
				{
				Certificate <Order_request> &oc = olog.fetch(i);
				oc.clear(); 						// Clear previous order requests if any    

				// Read requests and execute
				Status st(view(), i, GET_HOLE_TYPE);

				send_status(&st);
				}

			seqno				= max - 1;
			has_nv_state		= true;

			}


		void Replica::update_max_rec()
			{
			}


		void Replica::new_state(Seqno c)
			{
			}


		// Mark stable 
		// First phase : Locally commit 
		//	 1. Send order request message
		//	 2. Locally commit if we receive 2f+1 order request messages
		// Second phase : Checkpoint message
		//	 3.  Send checkpoint message after it is locally committed
		//	 4.  Recive 2f+1 matching checkpoint messages 
		// Garbage collect after checkpoint is complete
		void Replica::mark_stable(Seqno n)
			{

			if (n < last_stable)
				{
				// Do nothing if this is called before the request is ordered or if it is 
				// already garbage collected recently
				return;
				}

			// Garbage collect if the request has been already executed
			if (n <= last_executed)
				{
				//fprintf(stderr,"Marking stable :%lld last executed :%lld last ordered :%lld \n", n, last_executed,
				//max_request_ordered);
				// Garbage collect request history log
				last_stable 		= n;

				if (last_stable > low_bound)
					{
					low_bound			= last_stable;
					}

				rqueue.mark_stable(last_stable);
				rh_log.mark_stable(last_stable);

				// Garbage collect olog,clog,elog
				olog.truncate(last_stable);

				// Keep the current stable certificate for commit and checkpoint certificates
				elog.truncate(last_stable - 1);

				// View info 
				vi.mark_stable(last_stable);

				// Upcall into the application state
				exec_checkpoint_stable(last_stable);

				// Reset batch size
				batch_size			= max_batch_size;
				}
			else 
				{
				fprintf(stderr, "Marking stable without executing :%lld last executed :%lld last ordered :%lld \n", n,
					 last_executed, 
				max_request_ordered);
				}
			}



		void Replica::send_new_key()
			{
			Node::send_new_key();

			// TO DO Clean up messages in incomplete certificates 
			// that are authenticatd with old keys 
			}


		void Replica::send_status(Status * st)
			{

			send(st, primary());

			// TO DO (GET REQUEST TYPE): Send the status message to any random replica
			// and start the timer. If I do not get the response send it to some other
			// replica as follows. This reduces load on the primary in the presence of
			// lost packets

			/* int sn;
			 while (((sn = lrand48()%node->n()) == primary()) || (sn == id())) {
			 // do not send to primary or itself
			} 
			//fprintf(stderr, "send status %d %lld %lld to replica :%d \n", st->type(), 
			 // 			 st->snum(), last_stable,sn);
			send(st,sn); */
			// TO DO(GET HOLE TYPE): If I do not get the order request from the primary 
			// within a timeout. Start I_hate_primary timer and send it to rest of the 
			// replicas. If I get f+1 matching order requests then treat it as order 
			// request from the primary
			}


		bool Replica::shutdown()
			{
			return true;
			}


		bool Replica::restart(FILE * in)
			{
			return true;
			}


		void Replica::recover()
			{
			}


		void Replica::send_null()
			{
			th_assert(id() == primary(), "Invalid state");
			}


		//
		// Timeout handlers:
		//
		void itimer_handler()
			{
			th_assert(replica, "replica is not initialized\n");
			replica->send_hate_primary();
			}

		void vtimer_handler()
			{
			th_assert(replica, "replica is not initialized\n");
			replica->send_view_change();
			}

		void stimer_handler()
			{
			th_assert(replica, "replica is not initialized\n");

			//replica->send_status();
			replica->stimer->restart();
			}

		bool Replica::has_req(int cid, Digest & d)
			{
			Request *		req = rqueue.lookup(d);

			if (req && (req->client_id() == cid))
				return true;

			return false;
			}



		void Replica::join_mcast_group()
			{
			struct ip_mreq req;
			req.imr_multiaddr.s_addr = group->address()->sin_addr.s_addr;
			req.imr_interface.s_addr = INADDR_ANY;
			int 			error = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, 

			(char *) &		req, sizeof(req));

			if (error < 0)
				{
				perror("Unable to join group");
				exit(1);
				}
			}


		void Replica::leave_mcast_group()
			{
			struct ip_mreq req;
			req.imr_multiaddr.s_addr = group->address()->sin_addr.s_addr;
			req.imr_interface.s_addr = INADDR_ANY;
			int 			error = setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, 

			(char *) &		req, sizeof(req));

			if (error < 0)
				{
				perror("Unable to join group");
				exit(1);
				}
			}

