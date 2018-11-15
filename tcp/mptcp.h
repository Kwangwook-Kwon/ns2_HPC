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

#include "tcp-full.h"
#include "mptcp-full.h"
#include "agent.h"
#include "node.h"
#include "packet.h"
#include <vector>
#include "xpass.h"


#define MAX_SUBFLOW 100

typedef enum MP_SENDER_STATE_ {
  MP_SENDER_CLOSED=1,
  MP_SENDER_CLOSE_WAIT,
  MP_SENDER_CREDIT_REQUEST_SENT,
  MP_SENDER_CREDIT_RECEIVING,
  MP_SENDER_CREDIT_STOP_SENT,
  MP_SENDER_NSTATE,
} MP_SENDER_STATE;

class MptcpAgent;

class MP_FCT_Timer: public TimerHandler {
public:
  MP_FCT_Timer(MptcpAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  MptcpAgent *a_;
};

class MP_Waste_Timer: public TimerHandler {
public:
  MP_Waste_Timer(MptcpAgent *a): TimerHandler(), a_(a) { }
protected:
  virtual void expire(Event *);
  MptcpAgent *a_;
};

struct subflow
{
  subflow ():used (false), is_xpass(false), addr_ (0), port_ (0), daddr_ (-1), dport_ (-1),
    link_ (NULL), target_ (NULL), tcp_ (NULL), scwnd_ (0)
  {
  };
  bool used;
  bool is_xpass;
  int addr_;
  int port_;
  int daddr_;
  int dport_;
  NsObject *link_;
  NsObject *target_;
  MpFullTcpAgent *tcp_;
  XPassAgent *xpass_;
  int dstid_;
  double scwnd_;
};

struct dstinfo
{
  dstinfo ():addr_ (-1), port_ (-1), active_ (false)
  {
  };
  int addr_;
  int port_;
  bool active_;
};

struct dack_mapping
{
  int ackno;
  int length;
};

class MptcpAgent:public Agent
{
  friend class XcpEndsys;
  friend class MP_FCTTimer;
  virtual void sendmsg (int nbytes, const char *flags = 0);
public:
    MptcpAgent ();
   ~MptcpAgent ()
  {
  }
  void recv (Packet * pkt, Handler *);
  void set_dataack (int ackno, int length);
  int get_dataack ()
  {
    return mackno_;
  }
  double get_alpha ()
  {
    return alpha_;
  }
  double get_totalcwnd ()
  {
    totalcwnd_ = 0;
    for (int i = 0; i < sub_num_; i++) {
       totalcwnd_ += subflows_[i].tcp_->mptcp_get_cwnd ();
    }
    return totalcwnd_;
  }
  int command (int argc, const char *const *argv);
  void calculate_alpha ();
  TracedInt curseq_;
  double get_xpass_rtt();
  void handle_fct();
  void handle_waste();
  int find_low_rtt();
  MP_SENDER_STATE mp_sender_state_;

protected:
  virtual void delay_bind_init_all();
  virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);
  int get_subnum ();
  int find_subflow (int addr, int port);
  int find_subflow (int addr);
  void send_control ();
  void send_xpass();
  void add_destination (int addr, int port);
  bool check_routable (int sid, int addr, int port);
  inline double now() { return Scheduler::instance().clock(); }

  Classifier *core_;

  bool infinite_send_;
  seq_t credit_wasted;
  bool is_xpass;
  int is_sender_;
  int sub_num_;
  int dst_num_;
  seq_t remain_bytes_;
  seq_t total_bytes_;
  int remain_buffer_;
  seq_t flow_size_;
  int mcurseq_;
  int mackno_;
  int use_olia_;
  int fid_;
  int K;
  int act_sub_num_;
  int primary_subflow_;
  double fst_;
  double fct_;
  double default_credit_stop_timeout_;
  double totalcwnd_;
  double alpha_;
  struct subflow subflows_[MAX_SUBFLOW];
  struct dstinfo dsts_[MAX_SUBFLOW];
  vector < dack_mapping > dackmap_;
};
