#include "xpass.h"
#include "mptcp.h"
#include "ip.h"
#include "../tcp/template.h"

int hdr_xpass::offset_;
static class XPassHeaderClass : public PacketHeaderClass
{
public:
  XPassHeaderClass() : PacketHeaderClass("PacketHeader/XPass", sizeof(hdr_xpass))
  {
    bind_offset(&hdr_xpass::offset_);
  }
} class_xpass_hdr;

static class XPassClass : public TclClass
{
public:
  XPassClass() : TclClass("Agent/XPass") {}
  TclObject *create(int, const char *const *)
  {
    return (new XPassAgent());
  }
} class_xpass;

void SendCreditTimer::expire(Event *)
{
  a_->send_credit();
}

void CreditStopTimer::expire(Event *)
{
  a_->send_credit_stop();
}

void SenderRetransmitTimer::expire(Event *)
{
  a_->handle_sender_retransmit();
}

void ReceiverRetransmitTimer::expire(Event *)
{
  a_->handle_receiver_retransmit();
}

void FCTTimer::expire(Event *)
{
  a_->handle_fct();
}
void XPassAgent::delay_bind_init_all()
{
  delay_bind_init_one("max_credit_rate_");
  delay_bind_init_one("alpha_");
  delay_bind_init_one("min_credit_size_");
  delay_bind_init_one("max_credit_size_");
  delay_bind_init_one("min_ethernet_size_");
  delay_bind_init_one("max_ethernet_size_");
  delay_bind_init_one("xpass_hdr_size_");
  delay_bind_init_one("target_loss_scaling_");
  delay_bind_init_one("w_init_");
  delay_bind_init_one("min_w_");
  delay_bind_init_one("retransmit_timeout_");
  delay_bind_init_one("default_credit_stop_timeout_");
  delay_bind_init_one("min_jitter_");
  delay_bind_init_one("max_jitter_");
  delay_bind_init_one("reset_count_");
  delay_bind_init_one("reset_count2_");
  Agent::delay_bind_init_all();
}

void XPassAgent::trace(TracedVar *v)
{

  Agent::trace(v);
}

int XPassAgent::delay_bind_dispatch(const char *varName, const char *localName,
                                    TclObject *tracer)
{
  if (delay_bind(varName, localName, "max_credit_rate_", &max_credit_rate_,
                 tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "alpha_", &alpha_, tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "min_credit_size_", &min_credit_size_,
                 tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "max_credit_size_", &max_credit_size_,
                 tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "min_ethernet_size_", &min_ethernet_size_,
                 tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "max_ethernet_size_", &max_ethernet_size_,
                 tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "xpass_hdr_size_", &xpass_hdr_size_,
                 tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "target_loss_scaling_", &target_loss_scaling_,
                 tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "w_init_", &w_init_, tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "min_w_", &min_w_, tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "retransmit_timeout_", &retransmit_timeout_,
                 tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "default_credit_stop_timeout_", &default_credit_stop_timeout_,
                 tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "max_jitter_", &max_jitter_, tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "min_jitter_", &min_jitter_, tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "reset_count_", &reset_count_, tracer))
  {
    return TCL_OK;
  }
  if (delay_bind(varName, localName, "reset_count2_", &reset_count2_, tracer))
  {
    return TCL_OK;
  }
  return Agent::delay_bind_dispatch(varName, localName, tracer);
}

void XPassAgent::init()
{
  w_ = w_init_;
  //  cur_credit_rate_ = (int)(alpha_ * max_credit_rate_);
  last_credit_rate_update_ = now();
}

int XPassAgent::command(int argc, const char *const *argv)
{
  if (argc == 2)
  {
    if (strcmp(argv[1], "listen") == 0)
    {
      listen();
      return TCL_OK;
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
      //on_transmission_ = false;
      return TCL_OK;
    }
    else if (strcmp(argv[1], "close") == 0)
    {
      handle_fct();
      return TCL_OK;
    }
  }
  else if (argc == 3)
  {
    if (strcmp(argv[1], "advance-bytes") == 0)
    {
      //printf("advance-bytes calld : %d \n", argv[2]);
      if (credit_recv_state_ == XPASS_RECV_CLOSED)
      {
        advance_bytes(atol(argv[2]));
        return TCL_OK;
      }
      else
      {
        return TCL_ERROR;
      }
    }
  }
  return Agent::command(argc, argv);
}

void XPassAgent::recv(Packet *pkt, Handler *)
{
  hdr_cmn *cmnh = hdr_cmn::access(pkt);

  switch (cmnh->ptype())
  {
  case PT_XPASS_CREDIT_REQUEST:
    recv_credit_request(pkt);
    //printf("Packet Recieve :PT_XPASS_CREDIT_REQUEST \n");
    break;
  case PT_XPASS_CREDIT:
    recv_credit(pkt);
    break;
  case PT_XPASS_DATA:
    recv_data(pkt);
    break;
  case PT_XPASS_CREDIT_STOP:
    recv_credit_stop(pkt);
    break;
  case PT_XPASS_NACK:
    recv_nack(pkt);
    break;
  default:
    break;
  }
  Packet::free(pkt);
}

void XPassAgent::recv_credit_request(Packet *pkt)
{

  hdr_xpass *xph = hdr_xpass::access(pkt);
  switch (credit_send_state_)
  {
  case XPASS_SEND_CLOSE_WAIT:
    //fct_timer_.force_cancel();
  case XPASS_SEND_CLOSED:
    double lalpha;
    init();
    if (xph->sendbuffer_ >= 40 || xph->sendbuffer_ == 0)
    {
      lalpha = alpha_;
    }
    else
    {
      lalpha = alpha_ * xph->sendbuffer_ / 40.0;
    }
    //printf("sendbuffer : %d\n", xph->sendbuffer_);
    //printf("Curent lalpha : %f\n",lalpha);
    cur_credit_rate_ = (int)(lalpha * max_credit_rate_);
    fst_ = now(); //xph->credit_sent_time();
    //printf("Credit_requset_recieved! FST : %lf\n", fst_);
    // need to start to send credits.
    send_credit();

    // XPASS_SEND_CLOSED -> XPASS_SEND_CREDIT_REQUEST_RECEIVED
    credit_send_state_ = XPASS_SEND_CREDIT_SENDING;
    break;
  }
}

void XPassAgent::recv_credit(Packet *pkt)
{
  credit_recved_rtt_++;
  switch (credit_recv_state_)
  {
  case XPASS_RECV_CREDIT_REQUEST_SENT:
    sender_retransmit_timer_.force_cancel();
    credit_recv_state_ = XPASS_RECV_CREDIT_RECEIVING;
    // first sender RTT.
    rtt_ = now() - rtt_;
    last_credit_recv_update_ = now();
  case XPASS_RECV_CREDIT_RECEIVING:
    // send data
    if (datalen_remaining() > 0)
    {
      send(construct_data(pkt), 0);
    }

    if (datalen_remaining() == 0)
    {
      if (credit_stop_timer_.status() != TIMER_IDLE)
      {
        fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
        exit(1);
      }
      // Because ns2 does not allow sending two consecutive packets,
      // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
      credit_stop_timer_.sched(0);
    }
    else if (now() - last_credit_recv_update_ >= rtt_)
    {
      if (credit_recved_rtt_ >= (1 * pkt_remaining()))
      {
        // Early credit stop
        if (credit_stop_timer_.status() != TIMER_IDLE)
        {
          fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
          exit(1);
        }
        // Because ns2 does not allow sending two consecutive packets,
        // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
        credit_stop_timer_.sched(0);
      }
      credit_recved_rtt_ = 0;
      last_credit_recv_update_ = now();
    }
    break;
  case XPASS_RECV_CREDIT_STOP_SENT:
    if (datalen_remaining() > 0)
    {
      send(construct_data(pkt), 0);
    }
    else
    {
      credit_wasted_++;
    }
    credit_recved_++;
    break;
  case XPASS_RECV_CLOSE_WAIT:
    // accumulate credit count to check if credit stop has been delivered
    credit_wasted_++;
    break;
  case XPASS_RECV_CLOSED:
    credit_wasted_++;
    break;
  }
}

seq_t XPassAgent::recv_credit_mpath(Packet *pkt, int remain_bytes)
{
  //printf("MAX Segment : %d \n", max_segment());
  int send_datalen = (max_segment() > remain_bytes) ?  remain_bytes : max_segment();
  //printf("remain bytes :%d, send_datalen: %d State : %d\n" ,remain_bytes, send_datalen,credit_recv_state_ );
  credit_recved_rtt_++;
  switch (credit_recv_state_)
  {
  case XPASS_RECV_CREDIT_REQUEST_SENT:
    sender_retransmit_timer_.force_cancel();
    credit_recv_state_ = XPASS_RECV_CREDIT_RECEIVING;
    // first sender RTT.
    rtt_ = now() - rtt_;
    last_credit_recv_update_ = now();
  case XPASS_RECV_CREDIT_RECEIVING:
    // send data
    if (send_datalen > 0)
    {
      curseq_ += send_datalen;
      send(construct_data(pkt), 0);
      /*if (is_active_ == false)
      {
        credit_recv_state_ = XPASS_RECV_CREDIT_STOP_SENT;
      }else{
        credit_recv_state_ = XPASS_RECV_CREDIT_RECEIVING;
        sender_retransmit_timer_.force_cancel();
      }*/
    }

    /*    if (remain_bytes <= max_segment())
    {
      if (credit_stop_timer_.status() != TIMER_IDLE)
      {
        fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
        exit(1);
      }
      // Because ns2 does not allow sending two consecutive packets,
      // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
      credit_recv_state_ = XPASS_RECV_CREDIT_STOP_SENT;
      credit_stop_timer_.sched(0);
    }
    else if (now() - last_credit_recv_update_ >= rtt_)
    {
      if (credit_recved_rtt_ >= (1 * remain_bytes))
      {
        // Early credit stop
        if (credit_stop_timer_.status() != TIMER_IDLE)
        {
          fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
          exit(1);
        }
        // Because ns2 does not allow sending two consecutive packets,
        // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
        printf("Earl stop!!!  %fl\n",now());
        credit_recv_state_ = XPASS_RECV_CREDIT_STOP_SENT;
        credit_stop_timer_.sched(0);
      }
      credit_recved_rtt_ = 0;
      last_credit_recv_update_ = now();
    }*/
    break;
  case XPASS_RECV_CREDIT_STOP_SENT:
    if (send_datalen > 0)
    {
      curseq_ += send_datalen;
      send(construct_data(pkt), 0);
    }
    else
    {
      return 0;
      credit_wasted_++;
    }
    credit_recved_++;
    break;
  case XPASS_RECV_CLOSE_WAIT:
    // accumulate credit count to check if credit stop has been delivered
    credit_wasted_++;
    break;
  case XPASS_RECV_CLOSED:
    credit_wasted_++;
    break;
  }
  return send_datalen;
}

void XPassAgent::recv_data(Packet *pkt)
{
  //printf("recv DATA!!! FST : %lf\n" , fst_);
  if (fst_ == -1)
  { 
    fst_ = now();
    fprintf(stderr, "start fct from first data--------------------------------------\n");
  }
  fct_ = now() - fst_;
  hdr_xpass *xph = hdr_xpass::access(pkt);
  if (!xph->is_active)
  {
    send_credit_timer_.force_cancel();
    credit_send_state_ = XPASS_SEND_CLOSE_WAIT;
  }

  // distance between expected sequence number and actual sequence number.
  int distance = xph->credit_seq() - c_recv_next_;

  if (distance < 0)
  {
    // credit packet reordering or credit sequence number overflow happend.
    fprintf(stderr, "ERROR: Credit Sequence number is reverted.\n");
    exit(1);
  }
  credit_total_ += (distance + 1);
  credit_dropped_ += distance;
  credit_total_dropped_ += distance;
  c_recv_next_ = xph->credit_seq() + 1;
  recv_data_ += xph->data_length_;

  process_ack(pkt);
  update_rtt(pkt);
}

void XPassAgent::recv_nack(Packet *pkt)
{
  hdr_tcp *tcph = hdr_tcp::access(pkt);
  switch (credit_recv_state_)
  {
  case XPASS_RECV_CREDIT_STOP_SENT:
  case XPASS_RECV_CLOSE_WAIT:
  case XPASS_RECV_CLOSED:
    send(construct_credit_request(), 0);
    credit_recv_state_ = XPASS_RECV_CREDIT_REQUEST_SENT;
    sender_retransmit_timer_.resched(retransmit_timeout_);
  case XPASS_RECV_CREDIT_REQUEST_SENT:
  case XPASS_RECV_CREDIT_RECEIVING:
    // set t_seqno_ for retransmission
    t_seqno_ = tcph->ackno();
  }
}

void XPassAgent::recv_credit_stop(Packet *pkt)
{
  //fct_ = now() - fst_;
  //fct_timer_.resched(default_credit_stop_timeout_);
  send_credit_timer_.force_cancel();
  credit_send_state_ = XPASS_SEND_CLOSE_WAIT;
}

void XPassAgent::handle_fct()
{
  FILE *fct_out = fopen("outputs/fct.out", "a");

  fprintf(fct_out, "%d,%ld,%.10lf\n", fid_, recv_next_ - 1, fct_);
  fclose(fct_out);
  credit_send_state_ = XPASS_SEND_CLOSED;
}

void XPassAgent::handle_sender_retransmit()
{
  switch (credit_recv_state_)
  {
  case XPASS_RECV_CREDIT_REQUEST_SENT:
    send(construct_credit_request(), 0);
    sender_retransmit_timer_.resched(retransmit_timeout_);
    break;
  case XPASS_RECV_CREDIT_STOP_SENT:
    if (datalen_remaining() > 0)
    {
      credit_recv_state_ = XPASS_RECV_CREDIT_REQUEST_SENT;
      send(construct_credit_request(), 0);
      sender_retransmit_timer_.resched(retransmit_timeout_);
    }
    else
    {
      credit_recv_state_ = XPASS_RECV_CLOSE_WAIT;
      credit_recved_ = 0;
      sender_retransmit_timer_.resched((rtt_ > 0) ? rtt_ : default_credit_stop_timeout_);
    }
    break;
  case XPASS_RECV_CLOSE_WAIT:
    if (credit_recved_ == 0)
    {
      FILE *waste_out = fopen("outputs/waste.out", "a");

      credit_recv_state_ = XPASS_RECV_CLOSED;
      sender_retransmit_timer_.force_cancel();
      fprintf(waste_out, "%lf,%d,%ld,%d\n", now(),fid_, curseq_ - 1, credit_wasted_);
      fclose(waste_out);
      return;
    }
    // retransmit credit_stop
    printf(" stop 222222!!!  %fl\n", now());

    send_credit_stop();
    break;
  case XPASS_RECV_CLOSED:
    fprintf(stderr, "Sender Retransmit triggered while connection is closed.");
    exit(1);
  }
}

void XPassAgent::handle_receiver_retransmit()
{
  if (wait_retransmission_)
  {
    send(construct_nack(recv_next_), 0);
    receiver_retransmit_timer_.resched(retransmit_timeout_);
  }
}

Packet *XPassAgent::construct_credit_request()
{
  Packet *p = allocpkt();
  if (!p)
  {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }

  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);
  hdr_ip *iph = hdr_ip::access(p);

  tcph->seqno() = t_seqno_;
  tcph->ackno() = recv_next_;
  tcph->hlen() = xpass_hdr_size_;

  cmnh->size() = min_ethernet_size_;
  cmnh->ptype() = PT_XPASS_CREDIT_REQUEST;

  iph->prio() = fid_;

  xph->credit_seq() = 0;
  xph->credit_sent_time_ = now();
  if (remain_bytes_ > 0)
  {
    xph->sendbuffer_ = mpath_pkt_remaining();
  }
  else
  {
    xph->sendbuffer_ = pkt_remaining();
  }

  // to measure rtt between credit request and first credit
  // for sender.
  rtt_ = now();

  return p;
}

Packet *XPassAgent::construct_credit_stop()
{
  Packet *p = allocpkt();
  if (!p)
  {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);

  tcph->seqno() = t_seqno_;
  tcph->ackno() = recv_next_;
  tcph->hlen() = xpass_hdr_size_;

  cmnh->size() = min_ethernet_size_;
  cmnh->ptype() = PT_XPASS_CREDIT_STOP;

  xph->credit_seq() = 0;

  return p;
}

Packet *XPassAgent::construct_credit()
{
  Packet *p = allocpkt();
  if (!p)
  {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);
  int credit_size = min_credit_size_;

  if (min_credit_size_ < max_credit_size_)
  {
    // variable credit size
    credit_size += rand() % (max_credit_size_ - min_credit_size_ + 1);
  }
  else
  {
    // static credit size
    if (min_credit_size_ != max_credit_size_)
    {
      fprintf(stderr, "ERROR: min_credit_size_ should be less than or equal to max_credit_size_\n");
      exit(1);
    }
  }

  tcph->seqno() = t_seqno_;
  tcph->ackno() = recv_next_;
  tcph->hlen() = credit_size;

  cmnh->size() = credit_size;
  cmnh->ptype() = PT_XPASS_CREDIT;

  xph->credit_sent_time() = now();
  xph->credit_seq() = c_seqno_;
  xph->reset_ = reset_;

  c_seqno_ = max(1, c_seqno_ + 1);

  return p;
}

Packet *XPassAgent::construct_data(Packet *credit)
{
  Packet *p = allocpkt();
  if (!p)
  {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);
  hdr_xpass *credit_xph = hdr_xpass::access(credit);
  int datalen = (int)min(max_segment(),
                         datalen_remaining());

  if (datalen <= 0)
  {
    fprintf(stderr, "ERROR: datapacket has length of less than zero\n");
    exit(1);
  }
  tcph->seqno() = t_seqno_;
  tcph->ackno() = recv_next_;
  tcph->hlen() = xpass_hdr_size_;

  cmnh->size() = max(min_ethernet_size_, xpass_hdr_size_ + datalen);
  cmnh->ptype() = PT_XPASS_DATA;

  xph->credit_sent_time() = credit_xph->credit_sent_time();
  xph->credit_seq() = credit_xph->credit_seq();
  xph->data_length_ = datalen;
  xph->is_active = get_is_active();
  //printf("data_length : %d\n", datalen);

  t_seqno_ += datalen;

  return p;
}

Packet *XPassAgent::construct_data_mpath(Packet *credit,seq_t data_len)
{
  Packet *p = allocpkt();
  if (!p)
  {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);
  hdr_xpass *xph = hdr_xpass::access(p);
  hdr_xpass *credit_xph = hdr_xpass::access(credit);
  //int datalen = (int)min(max_segment(),
  //                       datalen_remaining());

  if (data_len <= 0)
  {
    fprintf(stderr, "ERROR: datapacket has length of less than zero\n");
    exit(1);
  }
  tcph->seqno() = t_seqno_;
  tcph->ackno() = recv_next_;
  tcph->hlen() = xpass_hdr_size_;

  cmnh->size() = max(min_ethernet_size_, xpass_hdr_size_ + data_len);
  cmnh->ptype() = PT_XPASS_DATA;

  xph->credit_sent_time() = credit_xph->credit_sent_time();
  xph->credit_seq() = credit_xph->credit_seq();
  xph->data_length_ = data_len;
  xph->is_active = get_is_active();

  t_seqno_ += data_len;

  return p;
}

Packet *XPassAgent::construct_nack(seq_t seq_no)
{
  Packet *p = allocpkt();
  if (!p)
  {
    fprintf(stderr, "ERROR: allockpkt() failed\n");
    exit(1);
  }
  hdr_tcp *tcph = hdr_tcp::access(p);
  hdr_cmn *cmnh = hdr_cmn::access(p);

  tcph->ackno() = seq_no;
  tcph->hlen() = xpass_hdr_size_;

  cmnh->size() = min_ethernet_size_;
  cmnh->ptype() = PT_XPASS_NACK;

  return p;
}

void XPassAgent::send_credit()
{
  //if(mp_agent_ -> mp_recv_state_ == MP_RECV_FINISHED)
  //  return;
  double avg_credit_size = (min_credit_size_ + max_credit_size_) / 2.0;
  double delay;
  credit_feedback_control();
  // send credit.
  send(construct_credit(), 0);
  // calculate delay for next credit transmission.
  delay = avg_credit_size / cur_credit_rate_;
  // add jitter
  if (max_jitter_ > min_jitter_)
  {
    double jitter = (double)rand() / (double)RAND_MAX;
    jitter = jitter * (max_jitter_ - min_jitter_) + min_jitter_;
    // jitter is in the range between min_jitter_ and max_jitter_
    delay = delay * (1 + jitter);
  }
  else if (max_jitter_ < min_jitter_)
  {
    fprintf(stderr, "ERROR: max_jitter_ should be larger than min_jitter_");
    exit(1);
  }
  send_credit_timer_.resched(delay);
  //printf("Send_credit()::delay            : %lf\n", delay);
  //printf("Send_credit()::avg_credit_size  : %lf\n", avg_credit_size );
  //printf("Send_credit()::cur_credit_rate  : %d\n", cur_credit_rate_ );
}

void XPassAgent::send_one_credit()
{
  printf("send one credit called!\n");
  send(construct_credit(), 0);
}

void XPassAgent::send_credit_stop()
{
  send(construct_credit_stop(), 0);
  // set on timer
  sender_retransmit_timer_.resched(rtt_ > 0 ? (2. * rtt_) : default_credit_stop_timeout_);
  credit_recv_state_ = XPASS_RECV_CREDIT_STOP_SENT; //Later changes to XPASS_RECV_CLOSE_WAIT -> XPASS_RECV_CLOSED
}

void XPassAgent::advance_bytes(seq_t nb)
{
  //printf("advance_bytes called\n");
  if (credit_recv_state_ != XPASS_RECV_CLOSED)
  {
    fprintf(stderr, "ERROR: tried to advance_bytes without XPASS_RECV_CLOSED\n");
  }
  if (nb <= 0)
  {
    fprintf(stderr, "ERROR: advanced bytes are less than or equal to zero\n");
  }

  // advance bytes
  curseq_ += nb;
  // send credit request
  send(construct_credit_request(), 0);
  sender_retransmit_timer_.sched(retransmit_timeout_);

  // XPASS_RECV_CLOSED -> XPASS_RECV_CREDIT_REQUEST_SENT
  credit_recv_state_ = XPASS_RECV_CREDIT_REQUEST_SENT;
}

void XPassAgent::send_credit_request(seq_t nb)
{
  remain_bytes_ += nb;
  send(construct_credit_request(), 0);
  sender_retransmit_timer_.sched(retransmit_timeout_);
  credit_recv_state_ = XPASS_RECV_CREDIT_REQUEST_SENT;
}

void XPassAgent::process_ack(Packet *pkt)
{
  hdr_cmn *cmnh = hdr_cmn::access(pkt);
  hdr_tcp *tcph = hdr_tcp::access(pkt);
  int datalen = cmnh->size() - tcph->hlen();
  if (datalen < 0)
  {
    fprintf(stderr, "ERROR: negative length packet has been detected.\n");
    exit(1);
  }
  if (tcph->seqno() > recv_next_)
  {
    printf("[%d] %lf: data loss detected. (expected = %ld, received = %ld)\n",
           fid_, now(), recv_next_, tcph->seqno());
    if (!wait_retransmission_)
    {
      wait_retransmission_ = true;
      send(construct_nack(recv_next_), 0);
      receiver_retransmit_timer_.resched(retransmit_timeout_);
    }
  }
  else if (tcph->seqno() == recv_next_)
  {
    if (wait_retransmission_)
    {
      wait_retransmission_ = false;
      receiver_retransmit_timer_.force_cancel();
    }
    recv_next_ += datalen;
  }
}

void XPassAgent::update_rtt(Packet *pkt)
{
  hdr_xpass *xph = hdr_xpass::access(pkt);

  double rtt = now() - xph->credit_sent_time();
  if (rtt_ > 0.0)
  {
    rtt_ = 0.8 * rtt_ + 0.2 * rtt;
  }
  else
  {
    rtt_ = rtt;
  }
}

void XPassAgent::credit_feedback_control()
{
  if (rtt_ <= 0.0)
  {
    return;
  }
  if ((now() - last_credit_rate_update_) < rtt_)
  {
    return;
  }
  if (credit_total_ == 0)
  {

    return;
  }

  int old_rate = cur_credit_rate_;
  double loss_rate = credit_dropped_ / (double)credit_total_;
  double target_loss = (1.0 - cur_credit_rate_ / (double)max_credit_rate_) * target_loss_scaling_;
  int min_rate = (int)(avg_credit_size() / rtt_);
  double temp;
  double temp_final;
  if (loss_rate > target_loss)
  {
    less_congested_ = 0;
    congestion_++;
    if (loss_rate >= 1.0)
    {
      cur_credit_rate_ = (int)(avg_credit_size() / rtt_);
    }
    else
    {
      cur_credit_rate_ = (int)(avg_credit_size() * (credit_total_ - credit_dropped_) / (now() - last_credit_rate_update_) * (1.0 + target_loss));
    }
    if (cur_credit_rate_ > old_rate)
    {
      cur_credit_rate_ = old_rate;
    }
    //original
    w_ = max(w_ / 2.0, min_w_);

    //stcp
    //w_ = max(w_ - w_*0.125, min_w_);

    can_increase_w_ = false;
  }
  else
  {
    less_congested_++;
    congestion_ = 0;
    //if (less_congested_ >= reset_count2_ && reset_count2_ > 0 && mp_agent_ -> get_act_sub_num() < mp_agent_ -> get_K())
    /*{
      less_congested_ = 0;
      //printf("reset subflow triggerd!\n");
      mp_agent_->reset_subflows();
    }*/
    // there is no congestion.
    if (can_increase_w_)
    {
      //ewtcp
      //temp = 0.70710678118*w_;
      //temp_final = (temp+1)*w_;

      //w_ = min(temp_final, 0.5);

      //original
      w_ = min((w_ * 0.5 + 0.25), 0.5);

      //stcp
      //w_ = min(w_+0.01, 0.5);
    }
    else
    {
      can_increase_w_ = true;
    }
    if (cur_credit_rate_ < max_credit_rate_)
    {
      cur_credit_rate_ = (int)(w_ * max_credit_rate_ + (1 - w_) * cur_credit_rate_);
    }
  }
  if (cur_credit_rate_ > max_credit_rate_)
  {
    cur_credit_rate_ = max_credit_rate_;
  }
  if (cur_credit_rate_ < min_rate)
  {
    cur_credit_rate_ = min_rate;
  }

  credit_total_ = 0;
  credit_dropped_ = 0;
  last_credit_rate_update_ = now();

  if ((congestion_ >= reset_count_ && reset_count_ > 0) ||
      (less_congested_ >= reset_count2_ && reset_count2_ > 0 && mp_agent_->get_act_sub_num() < mp_agent_->get_K()))
  {
    congestion_ = 0;
    less_congested_ = 0;
    //printf("reset subflow triggerd!\n");
    if(mp_agent_ -> mp_recv_state_ == MP_RECV_CREDIT_SENDING)
      mp_agent_->reset_subflows(fid_);
  }
  /*
printf("Credit_feedback_control()::old_rate         : %d\n", old_rate);
printf("Credit_feedback_control()::loss_rate        : %lf\n", loss_rate);
printf("Credit_feedback_control()::target_loss      : %lf\n", target_loss);
printf("Credit_feedback_control()::min_rate         : %d\n", min_rate);
printf("Credit_feedback_control()::rtt_             : %lf\n", rtt_);
printf("Credit_feedback_control()::w_               : %lf\n", w_);
printf("Credit_feedback_control()::cur_credit_rate_ : %d\n", cur_credit_rate_);
*/
}

bool XPassAgent::check_stop(int remain_bytes)
{
  if (remain_bytes == 0)
  {
    if (credit_stop_timer_.status() != TIMER_IDLE)
    {
      fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
      exit(1);
    }
    // Because ns2 does not allow sending two consecutive packets,
    // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
    return true;
  }
  else if (now() - last_credit_recv_update_ >= rtt_)
  {
    if (credit_recved_rtt_ >= (1 * remain_bytes))
    {
      // Early credit stop
      if (credit_stop_timer_.status() != TIMER_IDLE)
      {
        fprintf(stderr, "Error: CreditStopTimer seems to be scheduled more than once.\n");
        exit(1);
      }
      // Because ns2 does not allow sending two consecutive packets,
      // credit_stop_timer_ schedules CREDIT_STOP packet with no delay.
      printf("Earl stop!!!  %fl\n", now());
      return true;
    }
    credit_recved_rtt_ = 0;
    last_credit_recv_update_ = now();
  }
  return false;
}
