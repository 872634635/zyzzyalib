//已改
#ifndef _Message_tags_h
#define _Message_tags_h 1

//
// Each message type is identified by one of the tags in the set below.
//

const short Free_message_tag=0; // Used to mark free message reps.
                                // A valid message may never use this tag.
const short Request_tag=1;
const short Reply_tag=2;
const short Order_request_tag=3;
const short Local_commit_tag=4;
const short Committed_locally_tag=5;
const short Checkpoint_tag=6;
const short Status_tag=7;
const short View_change_tag=8;
const short New_view_tag=9;
const short View_change_ack_tag=10;
const short New_key_tag=11;
const short Hate_primary_tag=12;

const short Mycommit_tag=13;//改
const short Mycommitted_locally_tag=14;//改


#endif // _Message_tags_h
