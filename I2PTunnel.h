#ifndef I2PTUNNEL_H__
#define I2PTUNNEL_H__

#include <inttypes.h>
#include <string>
#include <set>
#include <memory>
#include <sstream>
#include <boost/asio.hpp>
#include "Identity.h"
#include "Destination.h"
#include "Datagram.h"
#include "Streaming.h"
#include "I2PService.h"

namespace i2p
{
namespace client
{
	const size_t I2P_TUNNEL_CONNECTION_BUFFER_SIZE = 8192;
	const int I2P_TUNNEL_CONNECTION_MAX_IDLE = 3600; // in seconds	
	const int I2P_TUNNEL_DESTINATION_REQUEST_TIMEOUT = 10; // in seconds
	// for HTTP tunnels		
	const char X_I2P_DEST_HASH[] = "X-I2P-DestHash"; // hash  in base64
	const char X_I2P_DEST_B64[] = "X-I2P-DestB64"; // full address in base64
	const char X_I2P_DEST_B32[] = "X-I2P-DestB32"; // .b32.i2p address

	class I2PTunnelConnection: public I2PServiceHandler, public std::enable_shared_from_this<I2PTunnelConnection>
	{
		public:

			I2PTunnelConnection (I2PService * owner, std::shared_ptr<boost::asio::ip::tcp::socket> socket,
				std::shared_ptr<const i2p::data::LeaseSet> leaseSet, int port = 0); // to I2P
			I2PTunnelConnection (I2PService * owner, std::shared_ptr<boost::asio::ip::tcp::socket> socket,
				std::shared_ptr<i2p::stream::Stream> stream); // to I2P using simplified API 
			I2PTunnelConnection (I2PService * owner, std::shared_ptr<i2p::stream::Stream> stream,  std::shared_ptr<boost::asio::ip::tcp::socket> socket, 
				const boost::asio::ip::tcp::endpoint& target, bool quiet = true); // from I2P
			~I2PTunnelConnection ();
			void I2PConnect (const uint8_t * msg = nullptr, size_t len = 0);
			void Connect ();
			
		protected:

			void Terminate ();	

			void Receive ();
			void HandleReceived (const boost::system::error_code& ecode, std::size_t bytes_transferred);	
			virtual void Write (const uint8_t * buf, size_t len); // can be overloaded
			void HandleWrite (const boost::system::error_code& ecode);	

			void StreamReceive ();
			void HandleStreamReceive (const boost::system::error_code& ecode, std::size_t bytes_transferred);
			void HandleConnect (const boost::system::error_code& ecode);

		private:

			uint8_t m_Buffer[I2P_TUNNEL_CONNECTION_BUFFER_SIZE], m_StreamBuffer[I2P_TUNNEL_CONNECTION_BUFFER_SIZE];
			std::shared_ptr<boost::asio::ip::tcp::socket> m_Socket;
			std::shared_ptr<i2p::stream::Stream> m_Stream;
			boost::asio::ip::tcp::endpoint m_RemoteEndpoint;
			bool m_IsQuiet; // don't send destination
	};

	class I2PTunnelConnectionHTTP: public I2PTunnelConnection
	{
		public:
			
			I2PTunnelConnectionHTTP (I2PService * owner, std::shared_ptr<i2p::stream::Stream> stream,
				std::shared_ptr<boost::asio::ip::tcp::socket> socket, 
				const boost::asio::ip::tcp::endpoint& target, const std::string& host); 

		protected:

			void Write (const uint8_t * buf, size_t len);

		private:
		
			std::string m_Host;
			std::stringstream m_InHeader, m_OutHeader;
			bool m_HeaderSent;
			std::shared_ptr<const i2p::data::IdentityEx> m_From;
	};

	class I2PTunnelConnectionIRC: public I2PTunnelConnection
    {
        public:
                
            I2PTunnelConnectionIRC (I2PService * owner, std::shared_ptr<i2p::stream::Stream> stream,
                std::shared_ptr<boost::asio::ip::tcp::socket> socket, 
                const boost::asio::ip::tcp::endpoint& target, const std::string& m_WebircPass); 

        protected:

            void Write (const uint8_t * buf, size_t len);

        private:
   
            std::shared_ptr<const i2p::data::IdentityEx> m_From;
            std::stringstream m_OutPacket, m_InPacket;
			bool m_NeedsWebIrc;
            std::string m_WebircPass; 
    };


	class I2PClientTunnel: public TCPIPAcceptor
	{
		protected:

			// Implements TCPIPAcceptor
			std::shared_ptr<I2PServiceHandler> CreateHandler(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

		public:

			I2PClientTunnel (const std::string& name, const std::string& destination, 
				const std::string& address, int port, std::shared_ptr<ClientDestination> localDestination, int destinationPort = 0);
			~I2PClientTunnel () {}
	
			void Start ();
			void Stop ();

			const char* GetName() { return m_Name.c_str (); }	
			
		private:

			const i2p::data::IdentHash * GetIdentHash ();

		private:
			
			std::string m_Name, m_Destination;
			const i2p::data::IdentHash * m_DestinationIdentHash;
			int m_DestinationPort;	
	};


	/** 2 minute timeout for udp sessions */
	const uint64_t I2P_UDP_SESSION_TIMEOUT = 1000 * 60 * 2;

	/** max size for i2p udp */
	const size_t I2P_UDP_MAX_MTU = i2p::datagram::MAX_DATAGRAM_SIZE;

	struct UDPSession
	{
		i2p::datagram::DatagramDestination * m_Destination;
		boost::asio::io_service & m_Service;
		boost::asio::ip::udp::socket IPSocket;
		i2p::data::IdentHash Identity;
		boost::asio::ip::udp::endpoint FromEndpoint;
		boost::asio::ip::udp::endpoint SendEndpoint;
		uint64_t LastActivity;

		uint16_t LocalPort;
		uint16_t RemotePort;

		uint8_t m_Buffer[I2P_UDP_MAX_MTU];
	
		UDPSession(boost::asio::ip::udp::endpoint localEndpoint,
							 const std::shared_ptr<i2p::client::ClientDestination> & localDestination,
							 boost::asio::ip::udp::endpoint remote, const i2p::data::IdentHash ident,
							 uint16_t ourPort, uint16_t theirPort);
		void HandleReceived(const boost::system::error_code & ecode, std::size_t len);
		void Receive();
	};

  
	/** read only info about a datagram session */
	struct DatagramSessionInfo
	{
		/** the name of this forward */
		std::string Name;
		/** ident hash of local destination */
		std::shared_ptr<const i2p::data::IdentHash> LocalIdent;
		/** ident hash of remote destination */
		std::shared_ptr<const i2p::data::IdentHash> RemoteIdent;
		/** ident hash of IBGW in use currently in this session or nullptr if none is set */
		std::shared_ptr<const i2p::data::IdentHash> CurrentIBGW;
		/** ident hash of OBEP in use for this session or nullptr if none is set */
		std::shared_ptr<const i2p::data::IdentHash> CurrentOBEP;
		/** i2p router's udp endpoint */
		boost::asio::ip::udp::endpoint LocalEndpoint;
		/** client's udp endpoint */
		boost::asio::ip::udp::endpoint RemoteEndpoint;
		/** how long has this converstation been idle in ms */
		uint64_t idle;
	};
	
	/** server side udp tunnel, many i2p inbound to 1 ip outbound */
	class I2PUDPServerTunnel
  {
		public:
			I2PUDPServerTunnel(const std::string & name,
				std::shared_ptr<i2p::client::ClientDestination> localDestination,
        const boost::asio::ip::address & localAddress,
				boost::asio::ip::udp::endpoint forwardTo, uint16_t port);
			~I2PUDPServerTunnel();
			/** expire stale udp conversations */
			void ExpireStale(const uint64_t delta=I2P_UDP_SESSION_TIMEOUT);
			void Start();
			const char * GetName() const { return m_Name.c_str(); }
			std::vector<std::shared_ptr<DatagramSessionInfo> > GetSessions();
		private:
			void HandleRecvFromI2P(const i2p::data::IdentityEx& from, uint16_t fromPort, uint16_t toPort, const uint8_t * buf, size_t len);
			UDPSession * ObtainUDPSession(const i2p::data::IdentityEx& from, uint16_t localPort, uint16_t remotePort);
		private:
			const std::string m_Name;
			const uint16_t LocalPort;
			boost::asio::ip::address m_LocalAddress;
			boost::asio::ip::udp::endpoint m_RemoteEndpoint;
			std::mutex m_SessionsMutex;
			std::vector<UDPSession*> m_Sessions;
			std::shared_ptr<i2p::client::ClientDestination> m_LocalDest;
	};

	class I2PUDPClientTunnel 
	{
		public:
			I2PUDPClientTunnel(const std::string & name, const std::string &remoteDest,
				boost::asio::ip::udp::endpoint localEndpoint, std::shared_ptr<i2p::client::ClientDestination> localDestination,
				uint16_t remotePort);
			~I2PUDPClientTunnel();
			void Start();
			const char * GetName() const { return m_Name.c_str(); }
			std::vector<std::shared_ptr<DatagramSessionInfo> > GetSessions();
			bool IsLocalDestination(const i2p::data::IdentHash & destination) const { return destination == m_LocalDest->GetIdentHash(); }
		private:
			void HandleRecvFromI2P(const i2p::data::IdentityEx& from, uint16_t fromPort, uint16_t toPort, const uint8_t * buf, size_t len);
			void TryResolving();
			const std::string m_Name;
			UDPSession * m_Session;
			const std::string m_RemoteDest;
			std::shared_ptr<i2p::client::ClientDestination> m_LocalDest;
			const boost::asio::ip::udp::endpoint m_LocalEndpoint;
			i2p::data::IdentHash * m_RemoteIdent;
			std::thread * m_ResolveThread;
			uint16_t LocalPort;
			uint16_t RemotePort;
			bool m_cancel_resolve;
	};
	
	class I2PServerTunnel: public I2PService
	{
		public:

			I2PServerTunnel (const std::string& name, const std::string& address, int port, 
				std::shared_ptr<ClientDestination> localDestination, int inport = 0, bool gzip = true);	

			void Start ();
			void Stop ();

			void SetAccessList (const std::set<i2p::data::IdentHash>& accessList); 

			const std::string& GetAddress() const { return m_Address; }
			int GetPort () const { return m_Port; };
			uint16_t GetLocalPort () const { return m_PortDestination->GetLocalPort (); };
			const boost::asio::ip::tcp::endpoint& GetEndpoint () const { return m_Endpoint; }

			const char* GetName() { return m_Name.c_str (); }	

      void SetMaxConnsPerMinute(const uint32_t conns) { m_PortDestination->SetMaxConnsPerMinute(conns); }

  private:

			void HandleResolve (const boost::system::error_code& ecode, boost::asio::ip::tcp::resolver::iterator it, 
				std::shared_ptr<boost::asio::ip::tcp::resolver> resolver);

			void Accept ();
			void HandleAccept (std::shared_ptr<i2p::stream::Stream> stream);
			virtual void CreateI2PConnection (std::shared_ptr<i2p::stream::Stream> stream);

		private:

			std::string m_Name, m_Address;
			int m_Port;
			boost::asio::ip::tcp::endpoint m_Endpoint;	
			std::shared_ptr<i2p::stream::StreamingDestination> m_PortDestination;
			std::set<i2p::data::IdentHash> m_AccessList;
			bool m_IsAccessList;
	};

	class I2PServerTunnelHTTP: public I2PServerTunnel
	{
		public:

			I2PServerTunnelHTTP (const std::string& name, const std::string& address, int port, 
				std::shared_ptr<ClientDestination> localDestination, const std::string& host,
			    int inport = 0, bool gzip = true);	

		private:

			void CreateI2PConnection (std::shared_ptr<i2p::stream::Stream> stream);

		private:

			std::string m_Host;
	};
	
    class I2PServerTunnelIRC: public I2PServerTunnel
    {
        public:

            I2PServerTunnelIRC (const std::string& name, const std::string& address, int port, 
                std::shared_ptr<ClientDestination> localDestination, const std::string& webircpass,
                int inport = 0, bool gzip = true);   

        private:

            void CreateI2PConnection (std::shared_ptr<i2p::stream::Stream> stream);

        private:

        	std::string m_WebircPass;
    };

}
}	

#endif
