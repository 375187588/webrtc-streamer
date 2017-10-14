/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** PeerConnectionManager.h
**
** -------------------------------------------------------------------------*/

#ifndef PEERCONNECTIONMANAGER_H_
#define PEERCONNECTIONMANAGER_H_

#include <string>

#include "api/peerconnectioninterface.h"
#include "api/test/fakeconstraints.h"

#include "rtc_base/logging.h"
#include "rtc_base/json.h"


class PeerConnectionManager {
	class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
		public:
			static SetSessionDescriptionObserver* Create(webrtc::PeerConnectionInterface* pc)
			{
				return  new rtc::RefCountedObject<SetSessionDescriptionObserver>(pc);
			}
			virtual void OnSuccess()
			{
				std::string sdp;
				if (m_pc->local_description())
				{
					m_pc->local_description()->ToString(&sdp);
					LOG(INFO) << __PRETTY_FUNCTION__ << " Local SDP:" << sdp;
				}
				if (m_pc->remote_description())
				{
					m_pc->remote_description()->ToString(&sdp);
					LOG(INFO) << __PRETTY_FUNCTION__ << " Remote SDP:" << sdp;
				}
			}
			virtual void OnFailure(const std::string& error)
			{
				LOG(LERROR) << __PRETTY_FUNCTION__ << " " << error;
			}
		protected:
			SetSessionDescriptionObserver(webrtc::PeerConnectionInterface* pc) : m_pc(pc) {};

		private:
			webrtc::PeerConnectionInterface* m_pc;
	};

	class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
		public:
			static CreateSessionDescriptionObserver* Create(webrtc::PeerConnectionInterface* pc)
			{
				return  new rtc::RefCountedObject<CreateSessionDescriptionObserver>(pc);
			}
			virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc)
			{
				std::string sdp;
				desc->ToString(&sdp);
				LOG(INFO) << __PRETTY_FUNCTION__ << " type:" << desc->type() << " sdp:" << sdp;
				m_pc->SetLocalDescription(SetSessionDescriptionObserver::Create(m_pc), desc);
			}
			virtual void OnFailure(const std::string& error) {
				LOG(LERROR) << __PRETTY_FUNCTION__ << " " << error;
			}
		protected:
			CreateSessionDescriptionObserver(webrtc::PeerConnectionInterface* pc) : m_pc(pc) {};

		private:
			webrtc::PeerConnectionInterface* m_pc;
	};

	class PeerConnectionStatsCollectorCallback : public webrtc::RTCStatsCollectorCallback {
		public:
			PeerConnectionStatsCollectorCallback() {}
			void clearReport() { m_report.clear(); }
			Json::Value getReport() { return m_report; }

		protected:
			virtual void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
				for (const webrtc::RTCStats& stats : *report) {
					Json::Value statsMembers;
					for (const webrtc::RTCStatsMemberInterface* member : stats.Members()) {
						statsMembers[member->name()] = member->ValueToString();
					}
					m_report[stats.id()] = statsMembers;
				}
			}

			Json::Value m_report;
	};

	class DataChannelObserver : public webrtc::DataChannelObserver  {
		public:
			DataChannelObserver(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel): m_dataChannel(dataChannel) {
				m_dataChannel->RegisterObserver(this);
			
			}
			virtual ~DataChannelObserver() {
				m_dataChannel->UnregisterObserver();
			}

			// DataChannelObserver interface
			virtual void OnStateChange() {
				LOG(LERROR) << __PRETTY_FUNCTION__ << " channel:" << m_dataChannel->label() << " state:"<< webrtc::DataChannelInterface::DataStateString(m_dataChannel->state());
				std::string msg(m_dataChannel->label() + " " + webrtc::DataChannelInterface::DataStateString(m_dataChannel->state()));
				webrtc::DataBuffer buffer(msg);
				m_dataChannel->Send(buffer);
			}
			virtual void OnMessage(const webrtc::DataBuffer& buffer) {
				std::string msg((const char*)buffer.data.data(),buffer.data.size());
				LOG(LERROR) << __PRETTY_FUNCTION__ << " channel:" << m_dataChannel->label() << " msg:" << msg;
			}

		protected:
			rtc::scoped_refptr<webrtc::DataChannelInterface>    m_dataChannel;
	};

	class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
		public:
			PeerConnectionObserver(PeerConnectionManager* peerConnectionManager, const std::string& peerid, const webrtc::PeerConnectionInterface::RTCConfiguration & config, const webrtc::FakeConstraints & constraints)
			: m_peerConnectionManager(peerConnectionManager)
			, m_peerid(peerid)
			, m_localChannel(NULL)
			, m_remoteChannel(NULL)
			, iceCandidateList_(Json::arrayValue) {
				m_pc = m_peerConnectionManager->peer_connection_factory_->CreatePeerConnection(config,
							    &constraints,
							    NULL,
							    NULL,
							    this);

				m_statsCallback = new rtc::RefCountedObject<PeerConnectionStatsCollectorCallback>();
				
				rtc::scoped_refptr<webrtc::DataChannelInterface>   channel = m_pc->CreateDataChannel("ServerDataChannel", NULL);
				m_localChannel = new DataChannelObserver(channel);
			};

			virtual ~PeerConnectionObserver() {
				LOG(INFO) << __PRETTY_FUNCTION__;
				delete m_localChannel;
				delete m_remoteChannel;
				m_pc->Close();
			}

			Json::Value getIceCandidateList() { return iceCandidateList_; 	}
			
			Json::Value getStats() {
				m_statsCallback->clearReport();
				m_pc->GetStats(m_statsCallback);
				int count=10;
				while ( (m_statsCallback->getReport().empty()) && (--count > 0) )
				{
					usleep(1000);
				}
				return Json::Value(m_statsCallback->getReport());
			};

			rtc::scoped_refptr<webrtc::PeerConnectionInterface> getPeerConnection() { return m_pc; };

			// PeerConnectionObserver interface
			virtual void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)    {}
			virtual void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}
			virtual void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
				LOG(LERROR) << __PRETTY_FUNCTION__;
				m_remoteChannel = new DataChannelObserver(channel);
			}
			virtual void OnRenegotiationNeeded()                              {}

			virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
			virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState state) {}
			virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state) {
				LOG(INFO) << __PRETTY_FUNCTION__ << " " << state  << " " << m_peerid;
				if ( (state == webrtc::PeerConnectionInterface::kIceConnectionFailed)
				   ||(state == webrtc::PeerConnectionInterface::kIceConnectionClosed) )
				{
					iceCandidateList_.clear();
					m_peerConnectionManager->hangUp(m_peerid);
				}
			}
			virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState) {}


		private:
			PeerConnectionManager* m_peerConnectionManager;
			const std::string m_peerid;
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_pc;
			DataChannelObserver*    m_localChannel;
			DataChannelObserver*    m_remoteChannel;
			Json::Value iceCandidateList_;
			rtc::scoped_refptr<PeerConnectionStatsCollectorCallback> m_statsCallback;
	};

	public:
		PeerConnectionManager(const std::string & stunurl, const std::string & turnurl, const std::list<std::string> & urlList);
		virtual ~PeerConnectionManager();

		bool InitializePeerConnection();

		const Json::Value getIceCandidateList(const std::string &peerid);
		const Json::Value addIceCandidate(const std::string &peerid, const Json::Value& jmessage);
		const Json::Value getVideoDeviceList();
		const Json::Value getAudioDeviceList();
		const Json::Value getMediaList();
		const Json::Value hangUp(const std::string &peerid);
		const Json::Value call(const std::string &peerid, const std::string & videourl, const std::string & audiourl, const std::string & options, const Json::Value& jmessage);
		const Json::Value getIceServers(const std::string& clientIp);
		const Json::Value getPeerConnectionList();
		const Json::Value getStreamList();
		const Json::Value createOffer(const std::string &peerid, const std::string & videourl, const std::string & audiourl, const std::string & options);
		void              setAnswer(const std::string &peerid, const Json::Value& jmessage);


	protected:
		PeerConnectionObserver*                 CreatePeerConnection(const std::string& peerid);
		bool                                    AddStreams(webrtc::PeerConnectionInterface* peer_connection, const std::string & videourl, const std::string & audiourl, const std::string & options);
		rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateVideoTrack(const std::string & videourl, const std::string & options);
		rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateAudioTrack(const std::string & audiourl, const std::string & options);
		bool                                    streamStillUsed(const std::string & streamLabel);

	protected:
		rtc::scoped_refptr<webrtc::AudioDeviceModule>                             audioDeviceModule_;
		rtc::scoped_refptr<webrtc::AudioDecoderFactory>                           audioDecoderfactory_;
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>                peer_connection_factory_;
		std::map<std::string, PeerConnectionObserver* >                           peer_connectionobs_map_;
		std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >  stream_map_;
		std::string                                                               stunurl_;
		std::string                                                               turnurl_;
		std::string                                                               turnuser_;
		std::string                                                               turnpass_;
		const std::list<std::string>                                              urlList_;
		std::map<std::string,std::string>                                         m_videoaudiomap;
};

#endif
