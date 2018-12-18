/*
 * Copyright (C) 2011 WIDE Project.  All rights reserved.
 *
 * Yoshifumi Nishida  <nishida@sfc.wide.ad.jp>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ip.h"
#include "flags.h"
#include "random.h"
#include "template.h"
#include "mptcp.h"
#include "xpass.h"

static class MptcpClass : public TclClass
{
public:
  MptcpClass() : TclClass("Agent/MPTCP")
  {
  }
  TclObject *create(int, const char *const *)
  {
    return (new MptcpAgent());
  }
} class_mptcp;

void MP_FCT_Timer::expire(Event *)
{
  a_->handle_fct();
}

void MP_Waste_Timer::expire(Event *)
{

  a_->handle_waste();
}

void MP_Reset_Timer::expire(Event *)
{
  if (a_->mp_recv_state_ == MP_RECV_PATH_RESET)
    a_->mp_recv_state_ = MP_RECV_CREDIT_SENDING;
}

void MP_Reset_0_Timer::expire(Event *)
{
  for (int i = 0; i < a_->sub_num_; i++)
  {
    a_->subflows_[i].xpass_->reset_ = 0;
  }
}

MptcpAgent::MptcpAgent() : Agent(PT_TCP), is_xpass(false), sub_num_(0), remain_bytes_(0), total_bytes_(0),
                           mcurseq_(1), mackno_(1), infinite_send_(false), remain_buffer_(0), fct_mptcp_(-1),
                           mp_recv_state_(MP_RECV_CLOSED), fid_(-1), mp_reset_timer(this), mp_reset_0_timer(this),
                           dst_num_(0), fct_data_(-1), fct_stop_(-1), fst_(-1), mp_sender_state_(MP_SENDER_CLOSED),
                           K(100), act_sub_num_(0), credit_wasted(-1), is_sender_(0), flow_size_(0)
{
}

void MptcpAgent::delay_bind_init_all()
{
  Agent::delay_bind_init_all();
  delay_bind_init_one("use_olia_");
  delay_bind_init_one("fid_");
  delay_bind_init_one("default_credit_stop_timeout_");
  delay_bind_init_one("K");
  delay_bind_init_one("is_sender_");
}

/* haven't implemented yet */
int MptcpAgent::delay_bind_dispatch(const char *varName,
                                    const char *localName,
                                    TclObject *tracer)
{
  if (delay_bind_bool(varName, localName, "use_olia_", &use_olia_, tracer))
  {
    return TCL_OK;
  }

  if (delay_bind(varName, localName, "fid_", &fid_, tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "default_credit_stop_timeout_", &default_credit_stop_timeout_, tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "K", &K, tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "is_sender_", &is_sender_, tracer))
  {
    return TCL_OK;
  }
  return Agent::delay_bind_dispatch(varName, localName, tracer);
}

#if 0
void
MptcpAgent::TraceAll ()
{
}

void
MptcpAgent::TraceVar (const char *cpVar)
{
}

void
MptcpAgent::trace (TracedVar * v)
{
  if (eTraceAll == TRUE)
    TraceAll ();
  else
    TraceVar (v->name ());
}
#endif

int MptcpAgent::command(int argc, const char *const *argv)
{
  if (argc == 2)
  {
    if (strcmp(argv[1], "listen") == 0)
    {
      if (is_xpass == false)
      {
        for (int i = 0; i < sub_num_; i++)
        {
          if (subflows_[i].tcp_->command(argc, argv) != TCL_OK)
            return (TCL_ERROR);
        }
      }
      else
      {
        for (int i = 0; i < sub_num_; i++)
        {
          if (subflows_[i].xpass_->command(argc, argv) != TCL_OK)
            return (TCL_ERROR);
        }
      }
      return (TCL_OK);
    }

    if (strcmp(argv[1], "reset") == 0)
    {
      /* reset used flag information */
      bool used_dst[dst_num_];
      for (int j = 0; j < dst_num_; j++)
        used_dst[j] = false;
      for (int i = 0; i < sub_num_; i++)
      {
        for (int j = 0; j < dst_num_; j++)
        {
          /* if this destination is already used by other subflow, don't use it */
          if (used_dst[j])
            continue;
          if (check_routable(i, dsts_[j].addr_, dsts_[j].port_))
          {
            subflows_[i].daddr_ = dsts_[j].addr_;
            subflows_[i].dport_ = dsts_[j].port_;
            if (is_xpass == false)
            {
              subflows_[i].tcp_->daddr() = dsts_[j].addr_;
              subflows_[i].tcp_->dport() = dsts_[j].port_;
            }
            else if (is_xpass == true)
            {
              subflows_[i].xpass_->daddr() = dsts_[j].addr_;
              subflows_[i].xpass_->dport() = dsts_[j].port_;
            }
            used_dst[j] = true;
            break;
          }
        }
      }
      if (is_xpass == false)
        subflows_[0].tcp_->mptcp_set_primary();
      return (TCL_OK);
    }
    if (strcmp(argv[1], "close") == 0)
    {
      if (is_sender_ == 1)
      {
        this->handle_waste();
      }
      else
      {
        this->handle_fct();
      }
      return (TCL_OK);
    }
  }
  if (argc == 3)
  {
    if (strcmp(argv[1], "attach-tcp") == 0)
    {
      int id = get_subnum();
      subflows_[id].tcp_ = (MpFullTcpAgent *)TclObject::lookup(argv[2]);
      subflows_[id].used = true;
      subflows_[id].addr_ = subflows_[id].tcp_->addr();
      subflows_[id].port_ = subflows_[id].tcp_->port();
      subflows_[id].tcp_->mptcp_set_core(this);
      sub_num_++;
      return (TCL_OK);
    }
    else if (strcmp(argv[1], "attach-xpass") == 0)
    {
      int id = get_subnum();
      subflows_[id].xpass_ = (XPassAgent *)TclObject::lookup(argv[2]);
      subflows_[id].used = true;
      subflows_[id].is_xpass = true;
      subflows_[id].addr_ = subflows_[id].xpass_->addr();
      subflows_[id].port_ = subflows_[id].xpass_->port();
      subflows_[id].xpass_->set_mp_agent(this);
      //subflows_[id].xpass_->xpass_set_core (this);
      sub_num_++;
      is_xpass = true;
      return (TCL_OK);
    }
    else if (strcmp(argv[1], "set-multihome-core") == 0)
    {
      core_ = (Classifier *)TclObject::lookup(argv[2]);
      if (core_ == NULL)
      {
        return (TCL_ERROR);
      }
      return (TCL_OK);
    }

    else if (strcmp(argv[1], "send-msg") == 0)
    {
      seq_t msg_len = atol(argv[2]);
      if (msg_len <= 0)
      {
        return (TCL_ERROR);
      }
      sendmsg(msg_len);
      return (TCL_OK);
    }
  }
  if (argc == 4)
  {
    if (strcmp(argv[1], "add-multihome-destination") == 0)
    {
      add_destination(atoi(argv[2]), atoi(argv[3]));
      return (TCL_OK);
    }
  }
  if (argc == 6)
  {
    if (strcmp(argv[1], "add-multihome-interface") == 0)
    {
      /* argv[2] indicates the addresses of the mptcp session */

      /* find the id for tcp bound to this address */
      int id = find_subflow(atoi(argv[2]));
      if (id < 0)
      {
        fprintf(stderr, "cannot find tcp bound to interface addr [%s]",
                argv[2]);
        return (TCL_ERROR);
      }
      if (subflows_[id].is_xpass == false)
        subflows_[id].tcp_->port() = atoi(argv[3]);
      else
        subflows_[id].xpass_->port() = atoi(argv[3]);
      subflows_[id].port_ = atoi(argv[3]);
      subflows_[id].target_ = (NsObject *)TclObject::lookup(argv[4]);
      subflows_[id].link_ = (NsObject *)TclObject::lookup(argv[5]);
      if (subflows_[id].target_ == NULL || subflows_[id].link_ == NULL)
        return (TCL_ERROR);

      return (TCL_OK);
    }
  }
  return (Agent::command(argc, argv));
}

int MptcpAgent::get_subnum()
{
  for (int i = 0; i < MAX_SUBFLOW; i++)
  {
    if (!subflows_[i].used)
      return i;
  }
  return -1;
}

int MptcpAgent::find_subflow(int addr, int port)
{
  for (int i = 0; i < MAX_SUBFLOW; i++)
  {
    if (subflows_[i].addr_ == addr && subflows_[i].port_ == port)
      return i;
  }
  return -1;
}

int MptcpAgent::find_subflow(int addr)
{
  for (int i = 0; i < MAX_SUBFLOW; i++)
  {
    if (subflows_[i].addr_ == addr)
      return i;
  }
  return -1;
}

void MptcpAgent::recv(Packet *pkt, Handler *h)
{
  hdr_ip *iph = hdr_ip::access(pkt);
  hdr_tcp *tcph = hdr_tcp::access(pkt);
  hdr_cmn *cmmh = hdr_cmn::access(pkt);
  hdr_xpass *xph = hdr_xpass::access(pkt);
  int mptcp_flag = tcph->flags();

  /* find subflow id from the destination address */
  int id = find_subflow(iph->daddr());
  if (id < 0)
  {
    fprintf(stderr,
            "MptcpAgent:recv() fatal error. cannot find destination\n");
    abort();
  }
  if (is_xpass == false)
  {
    if ((mptcp_flag & TH_SYN) && (fst_ == -1))
    {
      fst_ = now();
    }
    if (mptcp_flag & TH_PUSH)
    {
      if (cmmh->size() - tcph->hlen() > 0)
        flow_size_ += cmmh->size() - tcph->hlen();
      fct_mptcp_ = now() - fst_;
    }
    /* processing mptcp options */
    if (tcph->mp_capable())
    {
      /* if we receive mpcapable option, return the same option as response */
      subflows_[id].tcp_->mpcapable_ = true;
    }
    if (tcph->mp_join())
    {
      /* if we receive mpjoin option, return the same option as response */
      subflows_[id].tcp_->mpjoin_ = true;
    }
    if (tcph->mp_ack())
    {
      /* when we receive mpack, erase the acked record */
      subflows_[id].tcp_->mptcp_remove_mapping(tcph->mp_ack());
    }

    if (tcph->mp_dsn())
    {
      /* when we receive mpdata, update new mapping */
      subflows_[id].tcp_->mpack_ = true;
      subflows_[id].tcp_->mptcp_recv_add_mapping(tcph->mp_dsn(),
                                                 tcph->mp_subseq(),
                                                 tcph->mp_dsnlen());
    }
  }

  /* make sure packet will be return to the src addr of the packet */
  if (is_xpass == false)
  {
    subflows_[id].tcp_->daddr() = iph->saddr();
    subflows_[id].tcp_->dport() = iph->sport();
  }
  else
  {
    subflows_[id].xpass_->daddr() = iph->saddr();
    subflows_[id].xpass_->dport() = iph->sport();
  }
  /* call subflow's recv function */
  if (!is_xpass)
    subflows_[id].tcp_->recv(pkt, h);
  else
  {
    switch (cmmh->ptype())
    {
    case PT_XPASS_CREDIT_REQUEST:
      //if(fst_ == 0){
      //  fst_ = xph -> credit_sent_time();
      //}
      if (mp_recv_state_ == MP_RECV_FINISHED || mp_recv_state_ == MP_RECV_CREDIT_SENDING)
      {
        Packet::free(pkt);
        break;
      }
      if (mp_recv_state_ == MP_RECV_CLOSED)
        mp_recv_state_ = MP_RECV_PATH_RESET;
      if (fst_ == -1)
      {
        fst_ = now();
      }
      for (int i = 0; i < sub_num_; i++)
      {
        subflows_[i].xpass_->reset_ = 1;
        subflows_[i].xpass_->recv_credit_request(pkt);
      }
      mp_reset_timer.resched(0.1);
      mp_reset_0_timer.resched(0.05);
      if (!remain_buffer_)
        remain_buffer_ = xph->sendbuffer_;
      break;

    case PT_XPASS_CREDIT:
      recv_credit(pkt, id);
      break;

    case PT_XPASS_DATA:
      fct_data_ = now() - fst_;
      remain_buffer_--;
      flow_size_ += xph->data_length_;
      subflows_[id].xpass_->recv_data(pkt);
      break;

    case PT_XPASS_CREDIT_STOP:
      if (mp_recv_state_ == MP_RECV_CREDIT_SENDING ||mp_recv_state_ == MP_RECV_PATH_RESET )
        mp_recv_state_ = MP_RECV_FINISHED;
      fct_stop_ = now() - fst_;
      for (int i = 0; i < sub_num_; i++)
      {
        subflows_[i].xpass_->recv_credit_stop(pkt);
      }
      break;
    case PT_XPASS_NACK:
      subflows_[id].xpass_->recv_nack(pkt);
      break;
    default:
      break;
    }
    Packet::free(pkt);
  }

  if (!is_xpass)
    send_control();
}

/*
 * add possible destination address
 */
void MptcpAgent::add_destination(int addr, int port)
{
  for (int i = 0; i < MAX_SUBFLOW; i++)
  {
    if (dsts_[i].active_)
      continue;
    dsts_[i].addr_ = addr;
    dsts_[i].port_ = port;
    dsts_[i].active_ = true;
    dst_num_++;
    return;
  }
  fprintf(stderr, "fatal error. cannot add destination\n");
  abort();
}

/*
 * check if this subflow can reach to the specified address
 */
bool MptcpAgent::check_routable(int sid, int addr, int port)
{
  Packet *p = allocpkt();
  hdr_ip *iph = hdr_ip::access(p);
  iph->daddr() = addr;
  iph->dport() = port;
  bool
      result = (static_cast<Classifier *>(subflows_[sid].target_)->classify(p) > 0) ? true : false;
  Packet::free(p);

  return result;
}

void MptcpAgent::sendmsg(seq_t nbytes, const char * /*flags */)
{
  // printf("nbytes :%d, remain_bytes: %d, total_bytes %d\n", nbytes, remain_bytes_, total_bytes_);
  if (nbytes == -1)
  {
    infinite_send_ = true;
    total_bytes_ = TCP_MAXSEQ;
  }
  else
  { 
    input_flow_size_ = nbytes;
    remain_bytes_ = nbytes;
    total_bytes_ = nbytes;
  }
  if (!is_xpass)
    send_control();
  else
    send_xpass();
}

void MptcpAgent::send_xpass()
{
  credit_wasted = 0;
  for (int i = 0; i < sub_num_; i++)
  {
    subflows_[i].xpass_->set_deactive();
    subflows_[i].xpass_->credit_recv_state_ = XPASS_RECV_CREDIT_REQUEST_SENT;
  }
  reset_time_ = now();
  srand(remain_bytes_);
  primary_subflow_ = rand() % sub_num_;
  mp_sender_state_ = MP_SENDER_CREDIT_REQUEST_SENT;
  subflows_[primary_subflow_].xpass_->send_credit_request((seq_t)remain_bytes_);
}

/*
 * control sending data
 */
void MptcpAgent::send_control()
{
  // printf("Total bytes 1: %lld\n", total_bytes_);
  if (total_bytes_ > 0 || infinite_send_)
  {
    /* one round */
    bool slow_start = false;
    for (int i = 0; i < sub_num_; i++)
    {
      int mss = subflows_[i].tcp_->size();
      double cwnd = subflows_[i].tcp_->mptcp_get_cwnd() * mss;
      int ssthresh = subflows_[i].tcp_->mptcp_get_ssthresh() * mss;
      int maxseq = subflows_[i].tcp_->mptcp_get_maxseq();
      int backoff = subflows_[i].tcp_->mptcp_get_backoff();
      int highest_ack = subflows_[i].tcp_->mptcp_get_highest_ack();
      int dupacks = subflows_[i].tcp_->mptcp_get_numdupacks();

#if 1
      // we don't utlize a path which has lots of timeouts
      if (backoff >= 4)
        continue;
#endif

      /* too naive logic to calculate outstanding bytes? */
      int outstanding = maxseq - highest_ack - dupacks * mss;
      if (outstanding <= 0)
        outstanding = 0;
      if (cwnd < ssthresh)
      {
        /* allow only one subflow to do slow start at the same time */
        if (!slow_start)
        {
          slow_start = true;
          subflows_[i].tcp_->mptcp_set_slowstart(true);
        }
        else
#if 0
              subflows_[i].tcp_->mptcp_set_slowstart (false);
#else
          /* allow to do slow-start simultaneously */
          subflows_[i].tcp_->mptcp_set_slowstart(true);
#endif
      }
      seq_t sendbytes = cwnd - outstanding;
      seq_t sentbytes = 0;
      seq_t minbytes = 0;
      if (sendbytes < mss)
        continue;
      if (sendbytes > total_bytes_)
        sendbytes = total_bytes_;

      //     printf("sendbytes : %lld, mss: %d, min : %d\n", sendbytes, mss, min(sendbytes, mss) );
      //if (sendbytes > mss) sendbytes = mss;
      while (sendbytes > 0)
      {
        minbytes = sendbytes < mss ? sendbytes : mss;
        subflows_[i].tcp_->mptcp_add_mapping(mcurseq_, minbytes);
        subflows_[i].tcp_->sendmsg(minbytes);
        mcurseq_ += minbytes;
        sendbytes -= minbytes;
        sentbytes += minbytes; // min(sendbytes, mss);
      }

      if (!infinite_send_)
      {
        total_bytes_ -= sentbytes;
        //        printf("sentbytes : %lld %lld %lld\n",sentbytes,minbytes,min(sendbytes, mss) );
      }
#if 0
            if (!slow_start) {
              double cwnd_i = subflows_[i].tcp_->mptcp_get_cwnd ();
              /*
                  As recommended in 4.1 of draft-ietf-mptcp-congestion-05
                  Update alpha only if cwnd_i/mss_i != cwnd_new_i/mss_i.
              */
              if (abs(subflows_[i].tcp_->mptcp_get_last_cwnd () - cwnd_i) < 1) {
                  calculate_alpha ();
              }
              subflows_[i].tcp_->mptcp_set_last_cwnd (cwnd_i);
            }
#endif
    }
  }
  //  printf("Total bytes 2: %lld\n", total_bytes_);
}

/*
 *  calculate alpha based on the equation in draft-ietf-mptcp-congestion-05
 *
 *  Peforme ths following calculation

                                      cwnd_i
                                 max --------
                                  i         2
                                      rtt_i
             alpha = tot_cwnd * ----------------
                               /      cwnd_i \ 2
                               | sum ---------|
                               \  i   rtt_i  /


 */

void MptcpAgent::calculate_alpha()
{
  double max_i = 0.001;
  double sum_i = 0;
  double totalcwnd = 0;

  for (int i = 0; i < sub_num_; i++)
  {
#if 1
    int backoff = subflows_[i].tcp_->mptcp_get_backoff();
    // we don't utlize a path which has lots of timeouts
    if (backoff >= 4)
      continue;
#endif

    double rtt_i = subflows_[i].tcp_->mptcp_get_srtt();
    double cwnd_i = subflows_[i].tcp_->mptcp_get_cwnd();

    if (rtt_i < 0.000001) // too small. Let's not update alpha
      return;

    double tmp_i = cwnd_i / (rtt_i * rtt_i);
    if (max_i < tmp_i)
      max_i = tmp_i;

    sum_i += cwnd_i / rtt_i;
    totalcwnd += cwnd_i;
  }
  if (sum_i < 0.001)
    return;

  alpha_ = totalcwnd * max_i / (sum_i * sum_i);
}

/*
 * create ack block based on data ack information
 */
void MptcpAgent::set_dataack(int ackno, int length)
{
  bool found = false;
  vector<dack_mapping>::iterator it = dackmap_.begin();

  while (it != dackmap_.end())
  {
    struct dack_mapping *p = &*it;

    /* find matched block for this data */
    if (p->ackno <= ackno && p->ackno + p->length >= ackno &&
        p->ackno + p->length < ackno + length)
    {
      p->length = ackno + length - p->ackno;
      found = true;
      break;
    }
    else
      ++it;
  }

  /* if there's no matching block, add new one */
  if (!found)
  {
    struct dack_mapping tmp_map = {ackno, length};
    dackmap_.push_back(tmp_map);
  }

  /* re-calculate cumlative ack and erase old records */
  it = dackmap_.begin();
  while (it != dackmap_.end())
  {
    struct dack_mapping *p = &*it;
    if (mackno_ >= p->ackno && mackno_ <= p->ackno + p->length)
      mackno_ = ackno + length;
    if (mackno_ > p->ackno + p->length)
    {
      it = dackmap_.erase(it);
    }
    else
      ++it;
  }
}

void MptcpAgent::handle_fct()
{
  FILE *fct_out_data = fopen("outputs/mp_fct_data.out", "a");
  //int credit_totaldropped = 0;

  fprintf(fct_out_data, "%d,%lld,%.10lf,%d\n", fid_, flow_size_, fct_data_);
  /*for (int i = 0; i < sub_num_; i++)
  {
    fprintf(fct_out_data, "fid : %d, dropped: %5d, recved: %5ld\n", fid_,
            subflows_[i].xpass_->get_credit_total_dropped(), subflows_[i].xpass_->get_recv_data());
  }*/
  fclose(fct_out_data);

  FILE *fct_out_stop = fopen("outputs/mp_fct_stop.out", "a");
  fprintf(fct_out_stop, "%d,%lld,%.10lf\n", fid_, flow_size_, fct_stop_);
  fclose(fct_out_stop);

  FILE *fct_mptcp = fopen("outputs/mptcp_fct.out", "a");
  fprintf(fct_mptcp, "%d,%lld,%.10lf\n", fid_, flow_size_, fct_mptcp_);
  fclose(fct_mptcp);
}

void MptcpAgent::handle_waste()
{
  FILE *waste_out = fopen("outputs/mp_waste.out", "a");
  fprintf(waste_out, "%d,%lld,%lld\n", fid_, total_bytes_, credit_wasted);
  fclose(waste_out);
}

double MptcpAgent::get_xpass_rtt()
{
  double rtt = 0;
  for (int i = 0; i < sub_num_; i++)
  {
    if (subflows_[i].xpass_->get_rtt() > rtt)
      ;
    rtt = subflows_[i].xpass_->get_rtt();
  }
  return rtt;
}

int MptcpAgent::find_low_rtt()
{
  double rtt = 9999;
  int id;
  for (int i = 0; i < sub_num_; i++)
  {
    if (subflows_[i].xpass_->get_rtt() < rtt)
    {
      id = i;
      rtt = subflows_[i].xpass_->get_rtt();
    }
  }
  return id;
}

void MptcpAgent::reset_subflows()
{
  if (mp_recv_state_ == MP_RECV_PATH_RESET)
    return; //nothing doing it is alread reset
  mp_recv_state_ = MP_RECV_PATH_RESET;
  mp_reset_timer.resched(0.1);
  mp_reset_0_timer.resched(0.05);
  for (int i = 0; i < sub_num_; i++)
  {
    subflows_[i].xpass_->reset_ = 1;
    subflows_[i].xpass_->credit_send_state_ = XPASS_SEND_CREDIT_SENDING;
    subflows_[i].xpass_->send_credit_timer_.resched(0);
    //subflows_[i].xpass_->send_one_credit();
  }
}

void MptcpAgent::recv_credit(Packet *pkt, int id)
{
  int sent_bytes;
  hdr_xpass *xph = hdr_xpass::access(pkt);
  if (mp_sender_state_ != MP_SENDER_CREDIT_STOP_SENT)
  {
    if (subflows_[id].xpass_->check_stop(remain_bytes_))
    {
      mp_sender_state_ = MP_SENDER_CREDIT_STOP_SENT;
      subflows_[find_low_rtt()].xpass_->credit_stop_timer_.resched(0);
      for (int i = 0; i < sub_num_; i++)
      {
        subflows_[i].xpass_->credit_recv_state_ = XPASS_RECV_CREDIT_STOP_SENT;
        subflows_[i].xpass_->sender_retransmit_timer_.resched(
            subflows_[i].xpass_->rtt_ > 0 ? (2. * subflows_[i].xpass_->rtt_) : subflows_[i].xpass_->default_credit_stop_timeout_);
      }
    }
  }

  switch (mp_sender_state_)
  {
  case MP_SENDER_CREDIT_REQUEST_SENT:
    if (mp_sender_state_ == MP_SENDER_CREDIT_REQUEST_SENT)
    {
      mp_sender_state_ = MP_SENDER_PATH_RESET;
      if (id != primary_subflow_)
      {
        subflows_[primary_subflow_].xpass_->sender_retransmit_timer_.force_cancel();
        subflows_[primary_subflow_].xpass_->credit_recv_state_ = XPASS_RECV_CREDIT_RECEIVING;
      }
    }
    goto reset;

  case MP_SENDER_CREDIT_RECEIVING:
    if (xph->reset_ == 1)
    {
      reset_time_ = now();
      act_sub_num_ = 0;
      for (int i = 0; i < sub_num_; i++)
      {
        subflows_[i].xpass_->set_deactive();
      }
      mp_sender_state_ = MP_SENDER_PATH_RESET;
      goto reset;
    }

    sent_bytes = subflows_[id].xpass_->recv_credit_mpath(pkt, remain_bytes_);
    remain_bytes_ -= sent_bytes;
    if (sent_bytes == 0)
      credit_wasted++;
    break;

  reset:
  case MP_SENDER_PATH_RESET:
    //printf("RESET\n\n");
    if (xph->reset_ == 0 || subflows_[id].xpass_-> get_is_active())
    {
      sent_bytes = subflows_[id].xpass_->recv_credit_mpath(pkt, remain_bytes_);
      remain_bytes_ -= sent_bytes;
      if (sent_bytes == 0)
        credit_wasted++;
    }
    else if (act_sub_num_ >= K)
    {
      subflows_[id].xpass_->set_deactive();
      sent_bytes = subflows_[id].xpass_->recv_credit_mpath(pkt, remain_bytes_);
      remain_bytes_ -= sent_bytes;
      if (sent_bytes == 0)
        credit_wasted++;
    }
    else
    {
      act_sub_num_++;
      subflows_[id].xpass_->set_active();
      sent_bytes = subflows_[id].xpass_->recv_credit_mpath(pkt, remain_bytes_);
      remain_bytes_ -= sent_bytes;
      if (sent_bytes == 0)
        credit_wasted++;
    }
    if (K == act_sub_num_ || (now() - reset_time_) > 0.07){
      mp_sender_state_ = MP_SENDER_CREDIT_RECEIVING;
      //printf("Act_sub_NUM : %2d, Flow Size : %10ld, Remain Size : %10ld\n", act_sub_num_, input_flow_size_, remain_bytes_);
      if(act_sub_num_ == 0 ){
        fprintf(stderr, "ERROR: No active subflow\n");
        exit(1);
      }
    }
    break;

  case MP_SENDER_CREDIT_STOP_SENT:
    sent_bytes = subflows_[id].xpass_->recv_credit_mpath(pkt, remain_bytes_);
    remain_bytes_ -= sent_bytes;
    if (sent_bytes == 0)
      credit_wasted++;
    break;
  }
}
