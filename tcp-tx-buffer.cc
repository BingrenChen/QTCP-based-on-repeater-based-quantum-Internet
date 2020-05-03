/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010-2015 Adrian Sai-wah Tam
 * Copyright (c) 2016 Natale Patriciello <natale.patriciello@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Original author: Adrian Sai-wah Tam <adrian.sw.tam@gmail.com>
 */

#include <algorithm>
#include <iostream>

#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/tcp-option-ts.h"

#include "tcp-tx-buffer.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("TcpTxBuffer");
TcpTxItem::TcpTxItem ()
  : m_packet (0),
    m_lost (false),
    m_retrans (false),
    m_lastSent (Time::Min ()),
    m_sacked (false),
    m_qided (false)
{
}

TcpTxItem::TcpTxItem (const TcpTxItem &other)
  : m_packet (other.m_packet),
    m_lost (other.m_lost),
    m_retrans (other.m_retrans),
    m_lastSent (other.m_lastSent),
    m_sacked (other.m_sacked),
    m_qided (other.m_qided)
{
}

void
TcpTxItem::Print (std::ostream &os) const
{
  bool comma = false;

  if (m_lost)
    {
      os << "[lost]";
      comma = true;
    }
  if (m_retrans)
    {
      if (comma)
        {
          os << ",";
        }

      os << "[retrans]";
      comma = true;
    }
  if (m_sacked)
    {
      if (comma)
        {
          os << ",";
        }
      os << "[sacked]";
      comma = true;
    }
  if (comma)
    {
      os << ",";
    }
  os << "last sent: " << m_lastSent;
}

NS_OBJECT_ENSURE_REGISTERED (TcpTxBuffer);

TypeId
TcpTxBuffer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpTxBuffer")
    .SetParent<Object> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpTxBuffer> ()
    .AddAttribute ("HEADERSIZE", "Header size  bytes",
                   UintegerValue (16),
                   MakeUintegerAccessor (&TcpTxBuffer::m_qheader),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("REDSIZE", "Redundancy length bytes",
                   UintegerValue (24),
                   MakeUintegerAccessor (&TcpTxBuffer::m_red),
                   MakeUintegerChecker<uint16_t> ())
    .AddTraceSource ("UnackSequence",
                     "First unacknowledged sequence number (SND.UNA)",
                     MakeTraceSourceAccessor (&TcpTxBuffer::m_firstByteSeq),
                     "ns3::SequenceNumber32TracedValueCallback")
  ;
  return tid;
}

/* A user is supposed to create a TcpSocket through a factory. In TcpSocket,
 * there are attributes SndBufSize and RcvBufSize to control the default Tx and
 * Rx window sizes respectively, with default of 128 KiByte. The attribute
 * SndBufSize is passed to TcpTxBuffer by TcpSocketBase::SetSndBufSize() and in
 * turn, TcpTxBuffer:SetMaxBufferSize(). Therefore, the m_maxBuffer value
 * initialized below is insignificant.
 */
TcpTxBuffer::TcpTxBuffer (uint32_t n)
  : m_maxBuffer (32768), m_size (0), m_sentSize (0),m_appsentSize(0), m_stageSize(0),m_stagesentSize(0),m_stage(1),m_sendQseq(0), m_stageQseq(0),m_esQseq(0),m_qednum(0),m_appnum(0),m_qheader(0),m_red(0),m_firstByteSeq (n)
{
}

TcpTxBuffer::~TcpTxBuffer (void)
{
  PacketList::iterator it;

  for (it = m_sentList.begin (); it != m_sentList.end (); ++it)
    {
      TcpTxItem *item = *it;

      m_sentSize -= item->m_packet->GetSize ();
if(item->m_qided)
{
m_stagesentSize -= item->m_packet->GetSize ();
}
  else
{
    m_appsentSize -= item->m_packet->GetSize ();
}
      delete item;
    }

  for (it = m_appList.begin (); it != m_appList.end (); ++it)
    {
      TcpTxItem *item = *it;
      m_size -= item->m_packet->GetSize ();
      delete item;
    }

  for (it = m_qidList.begin (); it != m_qidList.end (); ++it)
    {
      TcpTxItem *item = *it;
      m_stageSize -= item->m_packet->GetSize ();
      delete item;
    }
}

SequenceNumber32
TcpTxBuffer::HeadSequence (void) const
{
  return m_firstByteSeq;
}

SequenceNumber32
TcpTxBuffer::TailSequence (void) const
{
  return m_firstByteSeq + SequenceNumber32 (m_size)+SequenceNumber32 (m_stageSize);//
}

uint32_t
TcpTxBuffer::Size (void) const
{
  return m_size+m_stageSize; //
}
uint32_t
TcpTxBuffer::AppSize (void) const
{
  return m_size; //
}
uint32_t
TcpTxBuffer::QidSize (void) const
{
  return m_stageSize; //
}
uint32_t
TcpTxBuffer::QidsentSize (void) const
{
  return m_stagesentSize; //
}
uint32_t
TcpTxBuffer::AppsentSize (void) const
{
  return m_appsentSize; //
}
uint32_t
TcpTxBuffer::qidnum (void) const
{
  return m_qednum; //
}
uint32_t
TcpTxBuffer::appnum (void) const
{
  return m_appnum; //
}
uint32_t
TcpTxBuffer::qideable (void) const
{
  return m_stage; //
}
uint32_t
TcpTxBuffer::MaxBufferSize (void) const
{
  return m_maxBuffer;
}

void
TcpTxBuffer::SetMaxBufferSize (uint32_t n)
{
  m_maxBuffer = n;
}


uint32_t
TcpTxBuffer::Available (void) const
{
  return m_maxBuffer - m_size- m_stageSize ;//- m_stageSize
}

void
TcpTxBuffer::SetHeadSequence (const SequenceNumber32& seq)
{
  NS_LOG_FUNCTION (this << seq);
  m_firstByteSeq = seq;

  // if you change the head with data already sent, something bad will happen
  NS_ASSERT (m_sentList.size () == 0);
  m_highestSack = std::make_pair (m_sentList.end (), SequenceNumber32 (0));
}
void
TcpTxBuffer::Encode(Ptr<Packet> p,Ptr<Packet> q)
{//Immitation adding redundancy in QED.
uint32_t packnum33;
packnum33=q->GetSize ();
uint8_t aaa[packnum33]={0},bbb[packnum33]={0};
p->CopyData(bbb,packnum33);
for(uint32_t i=0;i<packnum33;i++)
{
aaa[i]=49;
bbb[i]=bbb[i]+aaa[i];
}
Ptr<Packet> it = Create<Packet> (bbb,packnum33);
*q=*it;
//return true;
}
/*
bool
TcpTxBuffer::Decode(Ptr<Packet> p,Ptr<Packet> q,uint32_t m=0)
{//Immitation recovering data in QED.
uint32_t packnum22;

packnum22=p->GetSize ();
uint8_t bbb[packnum22];
p->CopyData(bbb,packnum22);
Ptr<Packet> it = Create<Packet> (bbb,packnum22-m);
//q=p;
return it;
}
*/
bool
TcpTxBuffer::Add (Ptr<Packet> p)
{
if(m_esQseq==0){m_esQseq=rand ();}
if(5*(p->GetSize ()+m_qheader+m_red) <= Available ())//
{
m_appnum++;
uint32_t packnum11;
packnum11=p->GetSize ();
uint8_t ccc[packnum11]={0};
Ptr<Packet> q=Create<Packet> (ccc,packnum11+m_red);
Encode(p,q); 
Addqed(q);
Addapp(p);
m_sendQseq+=1;
m_stageQseq+=1;
m_esQseq+=1;
 return true;
}

return false;
 
}



bool
TcpTxBuffer::Addapp (Ptr<Packet> p)
{


//Addqed(p);

  NS_LOG_FUNCTION (this << p);
  NS_LOG_INFO ("Try to append" << p->GetSize () << " bytes to window starting at "
                                << m_firstByteSeq << ", availSize=" << Available ());

  if (p->GetSize ()+m_qheader+m_red <= Available ())
    {
    
if (p->GetSize () > 0)
        {
          
        TcpTxItem *item = new TcpTxItem ();
item->m_qided=false;
uint32_t packnum,lsnum;
packnum=p->GetSize ()+m_qheader+m_red;

uint8_t packet1[packnum]={0};

lsnum=m_stageQseq;
	    packet1[0]='Q';
	    packet1[1]='T';
	    packet1[2]='C';
            packet1[3]='S';
	    packet1[4]=lsnum>>24;//m_qidsum>>24;
	    packet1[5]=lsnum>>16;//m_qidsum>>16;
	    packet1[6]=lsnum>>8;//m_qidsum>>8;
	    packet1[7]=lsnum>>0;//m_qidsum;
lsnum=m_sendQseq;
	    packet1[8]=lsnum>>24;//m_sendQseq>>24;
	    packet1[9]=lsnum>>16;//m_sendQseq>>16;
	    packet1[10]=lsnum>>8;//m_sendQseq>>8;
	    packet1[11]=lsnum>>0;//m_sendQseq;

	    packet1[12]=128+64+m_qheader/4;

            packet1[13]=0;
            packet1[14]=0;
            packet1[15]=0;

	       // Ptr<Packet> packet1 = Create<Packet> (12);

	       Ptr<Packet> it = Create<Packet> (packet1,m_qheader);

	    //   Ptr<Packet> pqid = Create<Packet> (packet1,12);
uint8_t packet2[packnum]={0};
               Ptr<Packet> appn = Create<Packet> (packet2,m_red);
it->AddAtEnd(p);
it->AddAtEnd(appn);

        //item->m_qided=false;
          item->m_packet = it->Copy ();
          m_appList.insert (m_appList.end (), item);
          m_size += it->GetSize ();

          NS_LOG_INFO ("Updated size=" << m_size  << ", lastSeq=" <<
                       m_firstByteSeq + SequenceNumber32 (m_size)+SequenceNumber32 (m_stageSize));
        }
      return true;
    }
  NS_LOG_WARN ("Rejected. Not enough room to buffer packet.");
  return false;
}
bool
TcpTxBuffer::Addqed (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);


  NS_LOG_INFO ("Try to append qid" << p->GetSize () << " bytes to window starting at "
                                << m_firstByteSeq << ", availSize=" << Available ());

  if (p->GetSize ()+m_qheader+m_red <= Available ())
    {
      if (p->GetSize () > 0)
        {
          TcpTxItem *item = new TcpTxItem ();
item->m_qided=true;
uint32_t lsnum;


uint8_t packet1[m_qheader]={0};


            packet1[0]='Q';//QTCP
	    packet1[1]='T';
	    packet1[2]='C';//CheckSum
            packet1[3]='S';
lsnum=m_esQseq;
	    packet1[4]=lsnum>>24;//m_qidsum>>24;
	    packet1[5]=lsnum>>16;//m_qidsum>>16;
	    packet1[6]=lsnum>>8;//m_qidsum>>8;
	    packet1[7]=lsnum>>0;//m_qidsum;

lsnum=m_stageQseq;
	    packet1[8]=lsnum>>24;//m_qidsum>>24;
	    packet1[9]=lsnum>>16;//m_qidsum>>16;
	    packet1[10]=lsnum>>8;//m_qidsum>>8;
	    packet1[11]=lsnum>>0;//m_qidsum;
	    
	   
	    packet1[12]=128+m_qheader/4;
            packet1[13]=0;
            packet1[14]=0;
            packet1[15]=0;
	    Ptr<Packet> pqid = Create<Packet> (packet1,m_qheader);
            pqid->AddAtEnd(p);





          item->m_packet = pqid->Copy();

          m_qidList.insert (m_qidList.end (), item);
          m_stageSize += pqid->GetSize ();

          NS_LOG_INFO ("Updated qidsize=" << m_stageSize  << ", lastSeq=" <<
                       m_firstByteSeq + SequenceNumber32 (m_size)+SequenceNumber32 (m_stageSize));
        }
      return true;
    }
  NS_LOG_WARN ("Rejected. Not enough room to buffer packet.");
  return false;
}
bool
TcpTxBuffer::Addqednew (TcpTxItem *p)//TcpTxItem *item //Ptr<Packet> p
{
  NS_LOG_FUNCTION (this << p);


      if (p->m_packet->GetSize () > 0)
        {
          TcpTxItem *item = new TcpTxItem ();
item->m_qided=true;
uint32_t packnum,lsnum;
packnum=p->m_packet->GetSize ();
uint8_t packet1[packnum]={0};

lsnum=m_esQseq;
	    packet1[0]='Q';
	    packet1[1]='T';
	    packet1[2]='C';
            packet1[3]='S';

	    packet1[4]=lsnum>>24;//m_qidsum>>24;
	    packet1[5]=lsnum>>16;//m_qidsum>>16;
	    packet1[6]=lsnum>>8;//m_qidsum>>8;
	    packet1[7]=lsnum>>0;//m_qidsum;

lsnum=m_stageQseq;
	    packet1[8]=lsnum>>24;//m_qidsum>>24;
	    packet1[9]=lsnum>>16;//m_qidsum>>16;
	    packet1[10]=lsnum>>8;//m_qidsum>>8;
	    packet1[11]=lsnum>>0;//m_qidsum;
	    
	   
	    packet1[12]=128+m_qheader/4;;

            packet1[13]=0;
            packet1[14]=0;
            packet1[15]=0;

m_esQseq+=1;
m_stageQseq+=1;
	       // Ptr<Packet> packet1 = Create<Packet> (12);
               Ptr<Packet> pqid = Create<Packet> (packet1,m_qheader);
uint8_t packet2[packnum-m_qheader]={0};
for(uint32_t i=0;i<packnum-m_qheader;i++)
{
packet2[i]=1;
}
	       Ptr<Packet> pqidp = Create<Packet> (packet2,packnum-m_qheader);
uint8_t packet3[packnum-m_qheader]={0};
                Ptr<Packet> pqidq = Create<Packet> (packet3,packnum-m_qheader);
Encode(pqidp,pqidq);
pqid->AddAtEnd(pqidq);
            item->m_packet = pqid->Copy ();
//packet->RemoveAtEnd(24);// clear APP head 12
p->m_packet->RemoveAtStart(packnum);//clear APP end 24
            p->m_packet=pqid->Copy ();
        //  m_qidList.push_front (item);
      //    m_sentList.insert (m_sentList.end (), item);
        //  m_stageSize += pqid->GetSize ();

     }
      return true;

}

uint32_t
TcpTxBuffer::SizeFromSequence (const SequenceNumber32& seq) const
{
  NS_LOG_FUNCTION (this << seq);
  // Sequence of last byte in buffer
  SequenceNumber32 lastSeq = TailSequence ();

  if (lastSeq >= seq)
    {
      return lastSeq - seq;
    }

  NS_LOG_ERROR ("Requested a sequence beyond our space (" << seq << " > " << lastSeq <<
                "). Returning 0 for convenience.");
  return 0;
}

Ptr<Packet>
TcpTxBuffer::CopyFromSequence (uint32_t numBytes, const SequenceNumber32& seq)
{
  NS_LOG_FUNCTION (this << numBytes << seq);

  if (m_firstByteSeq > seq)
    {
      NS_LOG_ERROR ("Requested a sequence number which is not in the buffer anymore");
      return Create<Packet> ();
    }

  // Real size to extract. Insure not beyond end of data
  uint32_t s = std::min (numBytes, SizeFromSequence (seq));
uint32_t s1=std::min (s, m_stageSize-m_stagesentSize);
//uint32_t s0=std::min (s, m_stageSize); 
uint32_t s3=std::min (s, m_size-m_appsentSize);
//uint32_t s2=std::min (s,m_size);

NS_LOG_INFO ("start   S1="<<s1<<"    S3="<<s3<<"  S="<<s);

NS_LOG_INFO ("m_stage= "<<m_stage<<" m_qednum="<<m_qednum<<" m_appnum="<<m_appnum);

if (s == 0)
    {
      return Create<Packet> ();
    }

  TcpTxItem *outItem = 0;

  if (m_firstByteSeq + m_sentSize >= seq + s)
    {
      // already sent this block completely
     outItem = GetTransmittedSegment (s, seq);
     NS_ASSERT (outItem != 0);

    
     outItem->m_retrans = true;
     NS_LOG_INFO (" outItem->m_retrans= "<<outItem->m_retrans<<"  outItem->m_qided= "<<outItem->m_retrans<<" outItem "<< outItem);
     NS_LOG_DEBUG ("Retransmitting11 [" << seq << ";" << seq + s << "|" << s <<
                    "] from " << *this);
    }
  else 
{

if(m_qednum>0)
{
m_stage=0;
s=s3;
}
else
{
m_stage=1;
s=s1;
}
NS_LOG_INFO ("end   S1="<<s1<<"    S3="<<s3<<"  S="<<s);
if (m_firstByteSeq + m_sentSize <= seq)
    {
      NS_ABORT_MSG_UNLESS (m_firstByteSeq + m_sentSize == seq,
                           "Requesting a piece of new data with an hole");
 // this is the first time we transmit this block
     NS_LOG_INFO ("S===="<<s);

      outItem = GetNewSegment (s);
      NS_ASSERT (outItem != 0);
      NS_ASSERT (outItem->m_retrans == false);

      NS_LOG_DEBUG ("New segment [" << seq << ";" << seq + s << "|" << s <<
                    "] from " << *this);
    }
  else if (m_firstByteSeq + m_sentSize > seq && m_firstByteSeq + m_sentSize < seq + s)
    {
      // Partial: a part is retransmission, the remaining data is new

      // Take the new data and move it into sent list

      uint32_t amount = seq + s - m_firstByteSeq.Get () - m_sentSize;
      NS_LOG_DEBUG ("Moving segment [" << m_firstByteSeq + m_sentSize << ";" <<
                    m_firstByteSeq + m_sentSize + amount <<"|" << amount <<
                    "] from " << *this);
 NS_LOG_INFO ("get new form  amout"<<amount);
      outItem = GetNewSegment (amount);
      NS_ASSERT (outItem != 0);

      // Now get outItem from the sent list (there will be a merge)
      return CopyFromSequence (numBytes, seq);
    }
}
  outItem->m_lost = false;
  outItem->m_lastSent = Simulator::Now ();

  Ptr<Packet> toRet = outItem->m_packet->Copy ();

  NS_LOG_INFO ("toRet->GetSize ()== "<<toRet->GetSize ()<<"  S== "<<s);
  NS_ASSERT (toRet->GetSize () == s);

  return toRet;
}

TcpTxItem*
TcpTxBuffer::GetNewSegment (uint32_t numBytes)
{
  NS_LOG_FUNCTION (this << numBytes);

if(m_stage==1 )//m_stage==1&&
        {
    SequenceNumber32 startOfQidList = m_firstByteSeq + m_stagesentSize;
NS_LOG_INFO ("qid m_firstByteSeq="<<m_firstByteSeq<<" m_stagesentSize=" <<m_stagesentSize);
  bool listEdited = false;
NS_LOG_INFO ("get new form qid=="<<m_stageSize<<"get new form qidsent=="<<m_stagesentSize);
  TcpTxItem *itemqid = GetPacketFromList (m_qidList, startOfQidList,
                                       numBytes, startOfQidList, &listEdited);

  (void) listEdited;
//NS_ASSERT (m_qednum=0);
  // Move item from qidList to SentList (should be the first, not too complex)
  PacketList::iterator itqid = std::find (m_qidList.begin (), m_qidList.end (), itemqid);
  NS_ASSERT (itqid != m_qidList.end ());
itemqid->m_qided=true;
  m_qidList.erase (itqid);//

//m_stageSize--;


//itemqid->m_qided=true;
//itemqid->m_sacked=false;
//Addqednew(itemqid->m_packet);
//Addqednew(itemqid);
  m_sentList.insert (m_sentList.end (), itemqid);
  m_sentSize += itemqid->m_packet->GetSize ();

  m_stagesentSize += itemqid->m_packet->GetSize ();
NS_LOG_INFO ("get new form qid=="<<m_stageSize<<"get new form qidsent=="<<m_stagesentSize);
 
//Addqed(itemqid->m_packet);
 m_stage=0; 
return itemqid;
        }

else
 {
    

SequenceNumber32 startOfAppList = m_firstByteSeq + m_appsentSize ;
NS_LOG_INFO (" app m_firstByteSeq== "<<m_firstByteSeq<<" m_appsentSize= "<<m_appsentSize);
  bool listEdited = false;
 NS_LOG_INFO (" get new form app11==  "<<m_size<<" get new form appsent==  "<<m_appsentSize);

  TcpTxItem *item = GetPacketFromList (m_appList, startOfAppList,
                                       numBytes, startOfAppList, &listEdited);

  (void) listEdited;

  // Move item from AppList to SentList (should be the first, not too complex)
  PacketList::iterator it = std::find (m_appList.begin (), m_appList.end (), item);
  NS_ASSERT (it != m_appList.end ());
  item->m_qided=false;
  //item->m_sacked=false;
  m_appList.erase (it);
  m_sentList.insert (m_sentList.end (), item);
  m_sentSize += item->m_packet->GetSize ();


  m_appsentSize += item->m_packet->GetSize ();
 NS_LOG_INFO (" get new form app22== "<<m_size<<" get new form appsent== "<<m_appsentSize);
       m_qednum-=1;
m_stage=1; 
m_appnum--;
return item;
        }
  
}



TcpTxItem*
TcpTxBuffer::GetTransmittedSegment (uint32_t numBytes, const SequenceNumber32 &seq)
{
  NS_LOG_FUNCTION (this << numBytes << seq);
  NS_ASSERT (seq >= m_firstByteSeq);
  NS_ASSERT (numBytes <= m_sentSize);

  bool listEdited = false;

  TcpTxItem *item = GetPacketFromList (m_sentList, m_firstByteSeq, numBytes, seq, &listEdited);
 

 if (listEdited && m_highestSack.second >= m_firstByteSeq)
    {

      m_highestSack = GetHighestSacked ();
  
     }

if(item->m_qided && item->m_lost&&item->m_retrans){Addqednew(item);} 
  return item;
}

std::pair <TcpTxBuffer::PacketList::const_iterator, SequenceNumber32>
TcpTxBuffer::GetHighestSacked () const
{
  NS_LOG_FUNCTION (this);

  PacketList::const_iterator it;
  std::pair <TcpTxBuffer::PacketList::const_iterator, SequenceNumber32> ret;
  SequenceNumber32 beginOfCurrentPacket = m_firstByteSeq;

  ret = std::make_pair (m_sentList.end (), SequenceNumber32 (0));

  for (it = m_sentList.begin (); it != m_sentList.end (); ++it)
    {
      const TcpTxItem *item = *it;
      if (item->m_sacked)
        {
          ret = std::make_pair (it, beginOfCurrentPacket);
        }
      beginOfCurrentPacket += item->m_packet->GetSize ();
    }

  return ret;
}


void
TcpTxBuffer::SplitItems (TcpTxItem &t1, TcpTxItem &t2, uint32_t size) const
{
  NS_LOG_FUNCTION (this << size);
  t1.m_packet = t2.m_packet->CreateFragment (0, size);
  t2.m_packet->RemoveAtStart (size);
  t1.m_qided = t2.m_qided;//qid flage
  t1.m_sacked = t2.m_sacked;
  t1.m_lastSent = t2.m_lastSent;
  t1.m_retrans = t2.m_retrans;
  t1.m_lost = t2.m_lost;

}

TcpTxItem*
TcpTxBuffer::GetPacketFromList (PacketList &list, const SequenceNumber32 &listStartFrom,
                                uint32_t numBytes, const SequenceNumber32 &seq,
                                bool *listEdited) const
{
  NS_LOG_FUNCTION (this << numBytes << seq);

  /*
   * Our possibilites are sketched out in the following:
   *
   *                    |------|     |----|     |----|
   * GetList (m_data) = |      | --> |    | --> |    |
   *                    |------|     |----|     |----|
   *
   *                    ^ ^ ^  ^
   *                    | | |  |         (1)
   *                  seq | |  numBytes
   *                      | |
   *                      | |
   *                    seq numBytes     (2)
   *
   * (1) seq and numBytes are the boundary of some packet
   * (2) seq and numBytes are not the boundary of some packet
   *
   * We can have mixed case (e.g. seq over the boundary while numBytes not).
   *
   * If we discover that we are in (2) or in a mixed case, we split
   * packets accordingly to the requested bounds and re-run the function.
   *
   * In (1), things are pretty easy, it's just a matter of walking the list and
   * defragment packets, if needed (e.g. seq is the beginning of the first packet
   * while maxBytes is the end of some packet next in the list).
   */

  Ptr<Packet> currentPacket = 0;
  TcpTxItem *currentItem = 0;
  TcpTxItem *outItem = 0;
  PacketList::iterator it = list.begin ();
  SequenceNumber32 beginOfCurrentPacket = listStartFrom;
// NS_ASSERT (5==5);
  
  while (it != list.end ())
    {
      currentItem = *it;
      currentPacket = currentItem->m_packet;

      // The objective of this snippet is to find (or to create) the packet
      // that begin with the sequence seq

      if (seq < beginOfCurrentPacket + currentPacket->GetSize ())
        {
          // seq is inside the current packet
          if (seq == beginOfCurrentPacket)
            {
              // seq is the beginning of the current packet. Hurray!
              outItem = currentItem;
              NS_LOG_INFO ("Current packet starts at seq " << seq <<
                           " ends at " << seq + currentPacket->GetSize ());
            }
          else if (seq > beginOfCurrentPacket)
            {
              // seq is inside the current packet but seq is not the beginning,
              // it's somewhere in the middle. Just fragment the beginning and
              // start again.
              NS_LOG_INFO ("we are at " << beginOfCurrentPacket <<
                           " searching for " << seq <<
                           " and now we recurse because packet ends at "
                                        << beginOfCurrentPacket + currentPacket->GetSize ());
              TcpTxItem *firstPart = new TcpTxItem ();
              SplitItems (*firstPart, *currentItem, seq - beginOfCurrentPacket);

              // insert firstPart before currentItem
              list.insert (it, firstPart);
              *listEdited = true;
 
              return GetPacketFromList (list, listStartFrom, numBytes, seq, listEdited);
            }
          else
            {
              NS_FATAL_ERROR ("seq < beginOfCurrentPacket: our data is before");
            }
        }
      else
        {
          // Walk the list, the current packet does not contain seq
          beginOfCurrentPacket += currentPacket->GetSize ();
          it++;
          continue;
        }

      NS_ASSERT (outItem != 0);

      // The objective of this snippet is to find (or to create) the packet
      // that ends after numBytes bytes. We are sure that outPacket starts
      // at seq.

      if (seq + numBytes <= beginOfCurrentPacket + currentPacket->GetSize ())
        {
          // the end boundary is inside the current packet
          if (numBytes == currentPacket->GetSize ())
            {
              // the end boundary is exactly the end of the current packet. Hurray!
              if (currentItem->m_packet == outItem->m_packet)
                {
                  // A perfect match!
                  return outItem;
                }
              else
                {
                  // the end is exactly the end of current packet, but
                  // current > outPacket in the list. Merge current with the
                  // previous, and recurse.
                  NS_ASSERT (it != list.begin ());
                  TcpTxItem *previous = *(--it);

                  list.erase (it);

                  MergeItems (*previous, *currentItem);
                  delete currentItem;
                  *listEdited = true;
 
                  return GetPacketFromList (list, listStartFrom, numBytes, seq, listEdited);
                }
            }
          else if (numBytes < currentPacket->GetSize ())
            {
              // the end is inside the current packet, but it isn't exactly
              // the packet end. Just fragment, fix the list, and return.
              TcpTxItem *firstPart = new TcpTxItem ();
              SplitItems (*firstPart, *currentItem, numBytes);

              // insert firstPart before currentItem
              list.insert (it, firstPart);
              *listEdited = true;

              return firstPart;
            }
        }
      else
        {
          // The end isn't inside current packet, but there is an exception for
          // the merge and recurse strategy...
          if (++it == list.end ())
            {
              // ...current is the last packet we sent. We have not more data;
              // Go for this one.
              NS_LOG_WARN ("Cannot reach the end, but this case is covered "
                           "with conditional statements inside CopyFromSequence."
                           "Something has gone wrong, report a bug");
              return outItem;
            }

          // The current packet does not contain the requested end. Merge current
          // with the packet that follows, and recurse
          TcpTxItem *next = (*it); // Please remember we have incremented it
                                   // in the previous if

          MergeItems (*currentItem, *next);
          list.erase (it);

          delete next;

          *listEdited = true;

          return GetPacketFromList (list, listStartFrom, numBytes, seq, listEdited);
        }
    }

  NS_FATAL_ERROR ("This point is not reachable");
}

void
TcpTxBuffer::MergeItems (TcpTxItem &t1, TcpTxItem &t2) const
{
  NS_LOG_FUNCTION (this);
  if (t1.m_sacked == true && t2.m_sacked == true)
    {
      t1.m_sacked = true;
    }
  else
    {
      t1.m_sacked = false;
    }

  if (t2.m_retrans == true && t1.m_retrans == false)
    {
      t1.m_retrans = true;
    }
  if (t1.m_lastSent < t2.m_lastSent)
    {
      t1.m_lastSent = t2.m_lastSent;
    }

  if (t2.m_lost)
    {
      t1.m_lost = true;
    }

  t1.m_packet->AddAtEnd (t2.m_packet);
}

void
TcpTxBuffer::DiscardUpTo (const SequenceNumber32& seq)
{
  NS_LOG_FUNCTION (this << seq);

  // Cases do not need to scan the buffer
  if (m_firstByteSeq >= seq)
    {
      NS_LOG_DEBUG ("Seq " << seq << " already discarded.");
      return;
    }

  // Scan the buffer and discard packets
  uint32_t offset = seq - m_firstByteSeq.Get ();  // Number of bytes to remove
  uint32_t pktSize;
  PacketList::iterator i = m_sentList.begin ();
  while (m_size>0&&m_size+m_stageSize >0  && offset > 0)//&& (m_stageSize!=m_stagesentSize)  m_stageSize&& m_size+m_stageSize  > 0
    {
NS_LOG_INFO("offset="<<offset<<" seq="<<seq<<"m_firstByteSeq="<<seq-offset);

       if (i == m_sentList.end ())
        {
Ptr<Packet> p = CopyFromSequence (offset, m_firstByteSeq);
          NS_ASSERT (p != 0);
          i = m_sentList.begin ();
          NS_ASSERT (i != m_sentList.end ());
        }  
      TcpTxItem *item = *i;
      Ptr<Packet> p = item->m_packet;
      pktSize = p->GetSize ();

      if (offset >= pktSize)
        { 
 
// This packet is behind the seqnum. Remove this packet from the buffer
         // m_qednum++; 
 m_sentSize -= pktSize;
if(item->m_qided)
{
         m_qednum++;
         m_stageSize -= pktSize;
        
 m_stagesentSize -= pktSize;
}
else
{
//m_appnum++;
         m_size -= pktSize;       
         m_appsentSize -= pktSize;

}
 
  
        
          offset -= pktSize;
          m_firstByteSeq += pktSize;
          i = m_sentList.erase (i);
          delete item;
          NS_LOG_INFO ("While removing up to " << seq <<
                       ".Removed one packet of size " << pktSize <<
                       " starting from " << m_firstByteSeq - pktSize <<
                       " remaining " << m_sentSize <<
                       ". size " << m_size << ". qidsize "<< m_stageSize<<"qidnum: "<<m_qednum);
        }
      else if (offset > 0)
        { // Part of the packet is behind the seqnum. Fragment
          pktSize -= offset;
          // PacketTags are preserved when fragmenting
          item->m_packet = item->m_packet->CreateFragment (offset, pktSize);

     m_sentSize -= offset;
 
  if(item->m_qided)
{
          m_qednum+=1; 
          m_stageSize -= offset;
        //  m_sentSize -= offset;
m_stagesentSize -= offset;
}
else
{           
   // m_appnum++;
         m_size -= offset;
       //  m_sentSize -= offset;
        m_appsentSize -= offset;
}         
  

          m_firstByteSeq += offset;
          NS_LOG_INFO ("Fragmented one packet by size " << offset <<
                       ", new size=" << pktSize);
          break;
        }
    }
  // Catching the case of ACKing a FIN
  if (m_size==0 && m_stageSize == 0 )
    {
      m_firstByteSeq = seq;
    }

  if (!m_sentList.empty ())
    {
      TcpTxItem *head = m_sentList.front ();
      if (head->m_sacked)
        {
          // It is not possible to have the UNA sacked; otherwise, it would
          // have been ACKed. This is, most likely, our wrong guessing
          // when crafting the SACK option for a non-SACK receiver.

          head->m_sacked = false;
        }
    }

  if (m_highestSack.second <= m_firstByteSeq)
    {
      m_highestSack = std::make_pair (m_sentList.end (), SequenceNumber32 (0));
    }

  NS_LOG_DEBUG ("Discarded up to " << seq);
 NS_LOG_DEBUG ("Discarded up to appsent " << m_appsentSize);
NS_LOG_DEBUG ("Discarded up to qidsent " << m_stagesentSize);
 NS_LOG_DEBUG ("Discarded up to appsize " << m_size);
NS_LOG_DEBUG ("Discarded up to qidsize " << m_stageSize);
  NS_LOG_LOGIC ("Buffer status after discarding data " << *this);
  NS_ASSERT (m_firstByteSeq >= seq);
}

bool
TcpTxBuffer::Update (const TcpOptionSack::SackList &list)
{
  NS_LOG_FUNCTION (this);

  bool modified = false;
  TcpOptionSack::SackList::const_iterator option_it;
  NS_LOG_INFO ("Updating scoreboard, got " << list.size () << " blocks to analyze");
  for (option_it = list.begin (); option_it != list.end (); ++option_it)
    {
      Ptr<Packet> current;
      TcpTxItem *item;
      const TcpOptionSack::SackBlock b = (*option_it);

      PacketList::iterator item_it = m_sentList.begin ();
      SequenceNumber32 beginOfCurrentPacket = m_firstByteSeq;

      while (item_it != m_sentList.end ())
        {
          item = *item_it;
          current = item->m_packet;

          // Check the boundary of this packet ... only mark as sacked if
          // it is precisely mapped over the option
          if (beginOfCurrentPacket >= b.first
              && beginOfCurrentPacket + current->GetSize () <= b.second)
            {
              if (item->m_sacked)
                {
                  NS_LOG_INFO ("Received block [" << b.first << ";" << b.second <<
                               ", checking sentList for block " << beginOfCurrentPacket <<
                               ";" << beginOfCurrentPacket + current->GetSize () <<
                               "], found in the sackboard already sacked");
                }
              else
                {
                  item->m_sacked = true;
                  NS_LOG_INFO ("Received block [" << b.first << ";" << b.second <<
                               ", checking sentList for block " << beginOfCurrentPacket <<
                               ";" << beginOfCurrentPacket + current->GetSize () <<
                               "], found in the sackboard, sacking");
                  if (m_highestSack.second <= beginOfCurrentPacket + current->GetSize ())
                    {
                      PacketList::iterator new_it = item_it;
                      m_highestSack = std::make_pair (++new_it, beginOfCurrentPacket+current->GetSize ());
                    }
                }
              modified = true;
            }
          else if (beginOfCurrentPacket + current->GetSize () > b.second)
            {
              // we missed the block. It's useless to iterate again; Say "ciao"
              // to the loop for optimization purposes
              NS_LOG_INFO ("Received block [" << b.first << ";" << b.second <<
                           ", checking sentList for block " << beginOfCurrentPacket <<
                           ";" << beginOfCurrentPacket + current->GetSize () <<
                           "], not found, breaking loop");
              break;
            }

          beginOfCurrentPacket += current->GetSize ();
          ++item_it;
        }
    }

  NS_ASSERT ((*(m_sentList.begin ()))->m_sacked == false);

  return modified;
}

bool
TcpTxBuffer::IsLost (const SequenceNumber32 &seq, const PacketList::const_iterator &segment,
                     uint32_t dupThresh, uint32_t segmentSize) const
{
  NS_LOG_FUNCTION (this << seq << dupThresh << segmentSize);
  uint32_t count = 0;
  uint32_t bytes = 0;
  PacketList::const_iterator it;
  TcpTxItem *item;
  Ptr<const Packet> current;
  SequenceNumber32 beginOfCurrentPacket = seq;

  //NS_LOG_INFO ("Checking if seq=" << seq << " is lost from the buffer ");

  if ((*segment)->m_lost == true)
    {
    NS_LOG_INFO ("seq1=" << seq << " is lost because of lost flag");
      return true;
    }
 
  if ((*segment)->m_sacked == true)
    {
     // NS_LOG_INFO ("qid1="<<(*segment)->m_qided);
NS_LOG_INFO ("seq2=" << seq << " is not lost because of sacked flag");
      return false;
    }

  // From RFC 6675:
  // > The routine returns true when either dupThresh discontiguous SACKed
  // > sequences have arrived above 'seq' or more than (dupThresh - 1) * SMSS bytes
  // > with sequence numbers greater than 'SeqNum' have been SACKed.  Otherwise, the
  // > routine returns false.
  for (it = segment; it != m_highestSack.first; ++it)
    {
      if (beginOfCurrentPacket >= m_highestSack.second)
        {
         //NS_LOG_INFO ("qid2="<<(*it)->m_qided);

         NS_LOG_INFO ("seq3=" << seq << " is not lost because there are no sacked segment ahead");
          return false;
        }

      item = *it;
      current = item->m_packet;

      if (item->m_sacked)
        {
          NS_LOG_INFO ("Segment [" << beginOfCurrentPacket << ", " <<
                       beginOfCurrentPacket+item->m_packet->GetSize () <<
                       "] found to be SACKed");
          ++count;
          bytes += current->GetSize ();
          if ((count >= dupThresh) || (bytes > (dupThresh-1) * segmentSize))
            {
              NS_LOG_INFO ("seq4=" << seq << " is lost because of 3 sacked blocks ahead");
              return true;
            }
        }

      beginOfCurrentPacket += current->GetSize ();
    }

  return false;
}

bool
TcpTxBuffer::IsLost (const SequenceNumber32 &seq, uint32_t dupThresh,
                     uint32_t segmentSize) const
{
  NS_LOG_FUNCTION (this << seq << dupThresh);

  SequenceNumber32 beginOfCurrentPacket = m_firstByteSeq;
  PacketList::const_iterator it;

  if (seq >= m_highestSack.second)
    {
      return false;
    }

  // This O(n) method is called only once, and outside this class.
  // It should not harm the performance
  for (it = m_sentList.begin (); it != m_sentList.end (); ++it)
    {
      // Search for the right iterator before calling IsLost()
      if (beginOfCurrentPacket >= seq)
        {

          return IsLost (beginOfCurrentPacket, it, dupThresh, segmentSize);
        }

      beginOfCurrentPacket += (*it)->m_packet->GetSize ();
    }

  return false;
}

bool
TcpTxBuffer::NextSeg (SequenceNumber32 *seq, uint32_t dupThresh,
                      uint32_t segmentSize, bool isRecovery) const
{
  NS_LOG_FUNCTION (this);

  /* RFC 6675, NextSeg definition.
   *
   * (1) If there exists a smallest unSACKed sequence number 'S2' that
   *     meets the following three criteria for determining loss, the
   *     sequence range of one segment of up to SMSS octets starting
   *     with S2 MUST be returned.
   *
   *     (1.a) S2 is greater than HighRxt.
   *
   *     (1.b) S2 is less than the highest octet covered by any
   *           received SACK.
   *
   *     (1.c) IsLost (S2) returns true.
   */
  PacketList::const_iterator it;
  TcpTxItem *item;
  SequenceNumber32 seqPerRule3;
  bool isSeqPerRule3Valid = false;
  SequenceNumber32 beginOfCurrentPkt = m_firstByteSeq;

  for (it = m_sentList.begin (); it != m_sentList.end (); ++it)
    {
      item = *it;

      // Condition 1.a , 1.b , and 1.c
      if (item->m_retrans == false && item->m_sacked == false)
        {
          if (IsLost (beginOfCurrentPkt, it, dupThresh, segmentSize))
            {
              *seq = beginOfCurrentPkt;
              return true;
            }
          else if (seqPerRule3.GetValue () == 0 && isRecovery)
            {
              isSeqPerRule3Valid = true;
              seqPerRule3 = beginOfCurrentPkt;
            }
        }

      // Nothing found, iterate
      beginOfCurrentPkt += item->m_packet->GetSize ();
    }

  /* (2) If no sequence number 'S2' per rule (1) exists but there
   *     exists available unsent data and the receiver's advertised
   *     window allows, the sequence range of one segment of up to SMSS
   *     octets of previously unsent data starting with sequence number
   *     HighData+1 MUST be returned.
   */
  if (SizeFromSequence (m_firstByteSeq + m_sentSize) > 0)
    {
      *seq = m_firstByteSeq + m_sentSize;
      return true;
    }

  /* (3) If the conditions for rules (1) and (2) fail, but there exists
   *     an unSACKed sequence number 'S3' that meets the criteria for
   *     detecting loss given in steps (1.a) and (1.b) above
   *     (specifically excluding step (1.c)), then one segment of up to
   *     SMSS octets starting with S3 SHOULD be returned.
   */
  if (isSeqPerRule3Valid)
    {
      *seq = seqPerRule3;
      return true;
    }

  /* (4) If the conditions for (1), (2), and (3) fail, but there exists
   *     outstanding unSACKed data, we provide the opportunity for a
   *     single "rescue" retransmission per entry into loss recovery.
   *     If HighACK is greater than RescueRxt (or RescueRxt is
   *     undefined), then one segment of up to SMSS octets that MUST
   *     include the highest outstanding unSACKed sequence number
   *     SHOULD be returned, and RescueRxt set to RecoveryPoint.
   *     HighRxt MUST NOT be updated.
   *
   * This point require too much interaction between us and TcpSocketBase.
   * We choose to not respect the SHOULD (allowed from RFC MUST/SHOULD definition)
   */
  return false;
}

uint32_t
TcpTxBuffer::GetRetransmitsCount (void) const
{
  NS_LOG_FUNCTION (this);
  PacketList::const_iterator it;
  TcpTxItem *item;
  uint32_t count = 0;
  for (it = m_sentList.begin (); it != m_sentList.end (); ++it)
    {
      item = *it;
      if (item->m_retrans)
        {
          count++;
        }
    }
  return count;
}

uint32_t
TcpTxBuffer::BytesInFlight (uint32_t dupThresh, uint32_t segmentSize) const
{
  PacketList::const_iterator it;
  TcpTxItem *item;
  uint32_t size =0; // "pipe" in RFC
  SequenceNumber32 beginOfCurrentPkt = m_firstByteSeq;

  // After initializing pipe to zero, the following steps are taken for each
  // octet 'S1' in the sequence space between HighACK and HighData that has not
  // been SACKed:
  for (it = m_sentList.begin (); it != m_sentList.end (); ++it)
    {
      item = *it;
      if (!item->m_sacked)
        {
          // (a) If IsLost (S1) returns false: Pipe is incremented by 1 octet.
          if (!IsLost (beginOfCurrentPkt, it, dupThresh, segmentSize))
            {
              size += item->m_packet->GetSize ();
            }
          // (b) If S1 <= HighRxt: Pipe is incremented by 1 octet.
          // (NOTE: we use the m_retrans flag instead of keeping and updating
          // another variable). Only if the item is not marked as lost
          else if (item->m_retrans && !item->m_lost)
            {
              size += item->m_packet->GetSize ();
            }
        }
      beginOfCurrentPkt += item->m_packet->GetSize ();
    }
NS_LOG_INFO ("BytesInFlight size=="<<size);
  return size;
}

void
TcpTxBuffer::ResetScoreboard ()
{
  NS_LOG_FUNCTION (this);

  PacketList::iterator it;
  SequenceNumber32 beginOfCurrentPkt = m_firstByteSeq;

  for (it = m_sentList.begin (); it != m_sentList.end (); ++it)
    {
      (*it)->m_sacked = false;
      beginOfCurrentPkt += (*it)->m_packet->GetSize ();
    }

  m_highestSack = std::make_pair (m_sentList.end (), SequenceNumber32 (0));
}

void
TcpTxBuffer::ResetSentList (uint32_t keepItems)
{
  NS_LOG_FUNCTION (this);
  TcpTxItem *item;
  NS_LOG_INFO ("THE SENTLIST HAS BEEN RESETED!!!");
  NS_LOG_INFO ("111m_sentList.size ()=="<<m_sentList.size ()<<"  keepItems== "<<keepItems);
  NS_LOG_INFO ("111m_appsentsize =="<<m_appsentSize <<"  qidsentsize== "<<m_stagesentSize<<" m_sentSize="<<m_sentSize);
  // Keep the head items; they will then marked as lost
  while (m_sentList.size () > keepItems)
    {
  NS_LOG_INFO ("222m_sentList.size ()=" << m_sentList.size () << " keepItems =="<<keepItems);
 NS_LOG_INFO ("222m_appsentsize =="<<m_appsentSize <<"  qidsentsize== "<<m_stagesentSize<<" m_sentSize="<<m_sentSize);
      item = m_sentList.back ();
      item->m_retrans = item->m_sacked = false;
if(item->m_qided)
{

        m_qidList.push_front (item); 
        m_sentList.pop_back ();
NS_LOG_INFO ("333m_sentList.size ()=" << m_sentList.size () << " keepItems =="<<keepItems);
 NS_LOG_INFO ("333m_appsentsize =="<<m_appsentSize <<"  qidsentsize== "<<m_stagesentSize<<" m_sentSize="<<m_sentSize);
  if (m_sentList.size () > 0)
    {
      item = m_sentList.back ();
      item->m_lost = true;
      item->m_sacked = false;
      item->m_retrans = false;


        m_stagesentSize-= item->m_packet->GetSize ();
        m_sentSize-= item->m_packet->GetSize ();
//m_appsentSize=0;
    }
  else
    {

      m_sentSize=0;
      m_stagesentSize=0;
    }
}
else
{
   m_appnum++;
m_qednum++;  //Transmitted data will consume one more entanglement.
        m_appList.push_front (item); 
        m_sentList.pop_back ();
NS_LOG_INFO ("444m_sentList.size ()=" << m_sentList.size () << " keepItems =="<<keepItems);
 NS_LOG_INFO ("444m_appsentsize =="<<m_appsentSize <<"  qidsentsize== "<<m_stagesentSize<<" m_sentSize="<<m_sentSize);
  if (m_sentList.size () > 0)
    {
      item = m_sentList.back ();
      item->m_lost = true;
      item->m_sacked = false;
      item->m_retrans = false;

m_appsentSize-= item->m_packet->GetSize ();
m_sentSize-= item->m_packet->GetSize ();
//m_stagesentSize=0;
    }
  else
    {

m_appsentSize=0;
      m_sentSize=0;
    } 
}

}
NS_LOG_INFO ("555m_sentList.size ()=" << m_sentList.size () << " keepItems =="<<keepItems);
 NS_LOG_INFO ("555m_appsentsize =="<<m_appsentSize <<"  qidsentsize== "<<m_stagesentSize<<" m_sentSize="<<m_sentSize); 

  m_highestSack = std::make_pair (m_sentList.end (), SequenceNumber32 (0));
}

void
TcpTxBuffer::ResetLastSegmentSent ()
{
  NS_LOG_FUNCTION (this);
  if (!m_sentList.empty ())
    {
      TcpTxItem *item = m_sentList.back ();

      m_sentList.pop_back ();
  m_sentSize -= item->m_packet->GetSize ();
if(!item->m_qided)
{
    
 m_appsentSize -= item->m_packet->GetSize ();
      m_appList.insert (m_appList.begin (), item);
}
else
{ 
   
 m_stagesentSize -= item->m_packet->GetSize ();
     m_qidList.insert (m_qidList.begin (), item);
}
    }
}

void
TcpTxBuffer::SetSentListLost ()
{
  NS_LOG_FUNCTION (this);

  PacketList::iterator it;

  for (it = m_sentList.begin (); it != m_sentList.end (); ++it)
    {
      (*it)->m_lost = true;
    }
}

bool
TcpTxBuffer::IsHeadRetransmitted () const
{
  NS_LOG_FUNCTION (this);

  if (m_sentSize == 0) //
    {
      return false;
    }

  NS_ASSERT (m_sentList.size () > 0);
  return m_sentList.front ()->m_retrans;
}

Ptr<const TcpOptionSack>
TcpTxBuffer::CraftSackOption (const SequenceNumber32 &seq, uint8_t available) const
{
  NS_LOG_FUNCTION (this);
  Ptr<TcpOptionSack> sackBlock = 0;
  SequenceNumber32 beginOfCurrentPacket = m_firstByteSeq;
  Ptr<Packet> current;
  TcpTxItem *item;

  NS_LOG_INFO ("Crafting a SACK block, available bytes: " << (uint32_t) available <<
               " from seq: " << seq << " buffer starts at seq " << m_firstByteSeq);

  PacketList::const_iterator it;
  if (m_highestSack.first == m_sentList.end ())
    {
      it = m_sentList.begin ();
    }
  else
    {
      it = m_highestSack.first;
      beginOfCurrentPacket = m_highestSack.second;
    }

  while (it != m_sentList.end ())
    {
      item = *it;
      current = item->m_packet;

      SequenceNumber32 endOfCurrentPacket = beginOfCurrentPacket + current->GetSize ();

      // The first segment could not be sacked.. otherwise would be a
      // cumulative ACK :)
      if (item->m_sacked || it == m_sentList.begin ())
        {
          NS_LOG_DEBUG ("Analyzing segment: [" << beginOfCurrentPacket <<
                        ";" << endOfCurrentPacket << "], not usable, sacked=" <<
                        item->m_sacked);
          beginOfCurrentPacket += current->GetSize ();
        }
      else if (seq > beginOfCurrentPacket)
        {
          NS_LOG_DEBUG ("Analyzing segment: [" << beginOfCurrentPacket <<
                        ";" << endOfCurrentPacket << "], not usable, sacked=" <<
                        item->m_sacked);
          beginOfCurrentPacket += current->GetSize ();
        }
      else
        {
          // RFC 2018: The first SACK block MUST specify the contiguous
          // block of data containing the segment which triggered this ACK.
          // Since we are hand-crafting this, select the first non-sacked block.
          sackBlock = CreateObject <TcpOptionSack> ();
          sackBlock->AddSackBlock (TcpOptionSack::SackBlock (beginOfCurrentPacket,
                                                             endOfCurrentPacket));
          NS_LOG_DEBUG ("Analyzing segment: [" << beginOfCurrentPacket <<
                        ";" << endOfCurrentPacket << "] and found to be usable");

          // RFC 2018: The data receiver SHOULD include as many distinct SACK
          // blocks as possible in the SACK option
          // The SACK option SHOULD be filled out by repeating the most
          // recently reported SACK blocks  that are not subsets of a SACK block
          // already included
          // This means go backward until we finish space and include already SACKed block
          while (sackBlock->GetSerializedSize () + 8 < available)
            {
              --it;

              if (it == m_sentList.begin ())
                {
                  return sackBlock;
                }

              item = *it;
              current = item->m_packet;
              endOfCurrentPacket = beginOfCurrentPacket;
              beginOfCurrentPacket -= current->GetSize ();
              sackBlock->AddSackBlock (TcpOptionSack::SackBlock (beginOfCurrentPacket,
                                                                 endOfCurrentPacket));
              NS_LOG_DEBUG ("Filling the option: Adding [" << beginOfCurrentPacket <<
                            ";" << endOfCurrentPacket << "], available space now : " <<
                            (uint32_t) (available - sackBlock->GetSerializedSize ()));
              NS_ASSERT (beginOfCurrentPacket > m_firstByteSeq);
            }

          return sackBlock;
        }

      ++it;
    }

  return sackBlock;
}

std::ostream &
operator<< (std::ostream & os, TcpTxBuffer const & tcpTxBuf)
{
  TcpTxBuffer::PacketList::const_iterator it;
  std::stringstream ss;
  SequenceNumber32 beginOfCurrentPacket = tcpTxBuf.m_firstByteSeq;
  uint32_t sentSize = 0, appSize = 0,qidSize=0,appsentSize=0,qidsentSize=0;

  Ptr<Packet> p;
  for (it = tcpTxBuf.m_sentList.begin (); it != tcpTxBuf.m_sentList.end (); ++it)
    {
      p = (*it)->m_packet;
      ss << "[" << beginOfCurrentPacket << ";"
         << beginOfCurrentPacket + p->GetSize () << "|" << p->GetSize () << "|";
      (*it)->Print (ss);
      ss << "]";

      sentSize += p->GetSize ();
if((*it)->m_qided)
{
qidsentSize += p->GetSize ();
}
else
{
appsentSize += p->GetSize ();
}

      beginOfCurrentPacket += p->GetSize ();
    }

  for (it = tcpTxBuf.m_appList.begin (); it != tcpTxBuf.m_appList.end (); ++it)
    {
      appSize += (*it)->m_packet->GetSize ();
    }
  for (it = tcpTxBuf.m_qidList.begin (); it != tcpTxBuf.m_qidList.end (); ++it)
    {
      qidSize += (*it)->m_packet->GetSize ();
    }

  os << "Sent list: " << ss.str () << ", size = " << tcpTxBuf.m_sentList.size () <<
    " Total size: " << tcpTxBuf.m_size+tcpTxBuf.m_stageSize <<
    " m_firstByteSeq = " << tcpTxBuf.m_firstByteSeq <<
    " m_sentSize = " << tcpTxBuf.m_sentSize;

   NS_ASSERT (sentSize == tcpTxBuf.m_sentSize);
   NS_ASSERT (tcpTxBuf.m_size+tcpTxBuf.m_stageSize - tcpTxBuf.m_sentSize == appSize+qidSize);

  return os;
}

} // namepsace ns3
