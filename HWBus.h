#ifndef	HW_BUS_H
#define	HW_BUS_H

/*
 *	HWBus.h -- A simple pin-accurate hardware bus model, converted from HWBus.sc provided by SCE
 *
 *	System-Level Architecture and Modeling Lab
 *	Department of Electrical and Computer Engineering
 *	The University of Texas at Austin 
 *
 * 	Author: Kamyar Mirzazad Barijough (kammirzazad@utexas.edu)
 *  Edited 10/10/22: Erika S. Alcorta added HardwareBusProtocolTLM
 */

#include <systemc.h>

// Simple hardware bus

#define ADDR_WIDTH	16u
#define DATA_WIDTH	32u

#if DATA_WIDTH == 32u
# define DATA_BYTES 4u
#elif DATA_WIDTH == 16u
# define DATA_BYTES 2u
#elif DATA_WIDTH == 8u
# define DATA_BYTES 1u
#else
# error "Invalid data width"
#endif

class	IIntrSend : virtual public sc_interface
{
	public:

	virtual	void	send(void) = 0;
};

class	IIntrRecv : virtual public sc_interface
{
	public:

	virtual void	receive(void) = 0;
};


/* ----- Physical layer, bus protocol ----- */

// Protocol primitives
class	IMasterHardwareBusProtocol : virtual public sc_interface
{
	public:

	virtual void	masterRead (const sc_bv<ADDR_WIDTH>& a, sc_bv<DATA_WIDTH>& d) = 0;
	virtual void	masterWrite(const sc_bv<ADDR_WIDTH>& a, const sc_bv<DATA_WIDTH>& d) = 0;
};

class	ISlaveHardwareBusProtocol : virtual public sc_interface
{
	public:

	virtual void	slaveRead (const sc_bv<ADDR_WIDTH>& a, sc_bv<DATA_WIDTH>& d) = 0;
	virtual void	slaveWrite(const sc_bv<ADDR_WIDTH>& a, const sc_bv<DATA_WIDTH>& d) = 0;
};

// Master protocol implementation
class	MasterHardwareBus : public IMasterHardwareBusProtocol, public sc_channel
{
	public:

	sc_in<bool>	ack;
	sc_out<bool>	ready;

	sc_out< sc_bv<ADDR_WIDTH> > A;
	sc_inout< sc_bv<DATA_WIDTH> > D;

	MasterHardwareBus(sc_module_name name) : sc_channel(name) {}

	void	masterWrite(const sc_bv<ADDR_WIDTH>& a, const sc_bv<DATA_WIDTH>& d)
	{
		A.write(a);
		D.write(d);
		wait(5000,SC_PS);

		ready.write(1);
		while(!ack.read()) wait(ack.default_event());

		wait(10000,SC_PS);

		ready.write(0);
		while(ack.read()) wait(ack.default_event());
	}

	void	masterRead(const sc_bv<ADDR_WIDTH>& a, sc_bv<DATA_WIDTH>& d)
	{
		A.write(a);
		wait(5000,SC_PS);

		ready.write(1);
		while(!ack.read()) wait(ack.default_event());

		d = D.read();
		wait(15000,SC_PS);

		ready.write(0);
		while(ack.read()) wait(ack.default_event());
	}
};


// Slave protocol implementation
class	SlaveHardwareBus : public ISlaveHardwareBusProtocol, public sc_channel
{
	public:

	sc_in<bool>	ready;
	sc_out<bool>	ack;

	sc_in< sc_bv<ADDR_WIDTH> > A;
	sc_inout< sc_bv<DATA_WIDTH> > D;

	SlaveHardwareBus(sc_module_name name) : sc_channel(name) {}

	void	slaveWrite(const sc_bv<ADDR_WIDTH>& a, const sc_bv<DATA_WIDTH>& d)
	{
		t1:	while(!ready.read()) wait(ready.default_event());

			if(a != A.read()) 
			{
				wait(1000,SC_PS); // avoid hanging from t2 to t1
				goto t1;
			}
			else
			{
				D.write(d);
				wait(12000,SC_PS);
			}

			ack.write(1);
			while(ready.read()) wait(ready.default_event());

			wait(7000,SC_PS);

			ack.write(0);
	}

	void	slaveRead(const sc_bv<ADDR_WIDTH>& a, sc_bv<DATA_WIDTH>& d)
	{
		t1:	while(!ready.read()) wait(ready.default_event());

			if(a != A.read()) 
			{
				wait(1000,SC_PS);  // avoid hanging from t2 to t1
				goto t1;
			}
			else 
			{
				d = D.read();
				wait(12000,SC_PS);
			}

			ack.write(1);
			while(ready.read()) wait(ready.default_event());

			wait(7000,SC_PS);

			ack.write(0);
	}
};

/* -----  Physical layer, interrupt handling ----- */

class	MasterHardwareSyncDetect : public IIntrRecv, public sc_channel 
{
	public:

	sc_in<bool> intr;

	MasterHardwareSyncDetect(sc_module_name name) : sc_channel(name) {}

	void	receive(void)
	{
		wait(intr.posedge_event());
	}
};

class	SlaveHardwareSyncGenerate : public IIntrSend, public sc_channel
{
	public:

	sc_out<bool> intr;

	SlaveHardwareSyncGenerate(sc_module_name name) : sc_channel(name) {}

	void	send(void)
	{
		intr.write(1);
		wait(5000,SC_PS);
		intr.write(0);
	}
};

class HardwareBusProtocolTLM : public IMasterHardwareBusProtocol, public ISlaveHardwareBusProtocol, public sc_channel
{
  sc_bv<ADDR_WIDTH> bus_addr;
  sc_bv<DATA_WIDTH> bus_data;
  sc_bv<ADDR_WIDTH> bus_slave_wait_addr;

  sc_event ready, ack;

public:
  HardwareBusProtocolTLM(sc_module_name name) : sc_channel(name) {}
  
  void masterWrite(const sc_bv<ADDR_WIDTH>& a, const sc_bv<DATA_WIDTH>& d) 
  {
	bus_addr = a;
	bus_data = d;
	wait(5000, SC_PS);
	ready.notify(SC_ZERO_TIME);
	wait(ack);
	wait(10000, SC_PS);
  }

  void masterRead(const sc_bv<ADDR_WIDTH>& a, sc_bv<DATA_WIDTH>& d)
  {
	bus_addr = a;
	wait(5000, SC_PS);
	ready.notify(SC_ZERO_TIME);
	wait(ack);
	d = bus_data;
	wait(15000,SC_PS);
  }

  void slaveWrite(const sc_bv<ADDR_WIDTH>& a, const sc_bv<DATA_WIDTH>& d)
  {
	t1: wait(ready);
	if (a != bus_addr)  {
		goto t1;
	}	
	else {
		bus_data = d;
		wait(12000,SC_PS);
	}
	wait(7000,SC_PS);
	ack.notify(SC_ZERO_TIME);
  }
	
  void slaveRead(const sc_bv<ADDR_WIDTH>& a, sc_bv<DATA_WIDTH>& d) 
  {
	t1: wait(ready);
	if (a != bus_addr) {
		goto t1;
	}	
	else {
		d = bus_data;
		wait(12000,SC_PS);
	}
	wait(7000,SC_PS);
	ack.notify(SC_ZERO_TIME);
	}
	
};

/* -----  Media access layer ----- */

class	IMasterHardwareBusLinkAccess : virtual public sc_interface
{
	public:

	virtual void	MasterRead(int addr, void *data, unsigned long len) = 0;
	virtual void	MasterWrite(int addr, const void* data, unsigned long len) = 0;
};
  
class	ISlaveHardwareBusLinkAccess : virtual public sc_interface
{
	public:

	virtual void	SlaveRead(int addr, void *data, unsigned long len) = 0;
	virtual void	SlaveWrite(int addr, const void* data, unsigned long len) = 0;
};

class	MasterHardwareBusLinkAccess : public IMasterHardwareBusLinkAccess, public sc_channel
{
	public:

	sc_port<IMasterHardwareBusProtocol> protocol;

	MasterHardwareBusLinkAccess(sc_module_name name)  : sc_channel(name) {}

	void	MasterWrite(int addr, const void* data, unsigned long len)
	{
		unsigned long i;
		unsigned char *p;
		sc_uint<DATA_WIDTH> word = 0;
   
		for(p = (unsigned char*)data, i = 0; i < len; i++, p++)
		{
			word = (word<<8) + *p;
      
			if(!((i+1)%DATA_BYTES)) 
			{
				protocol->masterWrite(addr, word);
				word = 0;
      			}
		}
    
		if(i%DATA_BYTES)
		{
			word <<= 8 * (DATA_BYTES - (i%DATA_BYTES));
			protocol->masterWrite(addr, word);
		}
	}
  
	void	MasterRead(int addr, void* data, unsigned long len)
	{
		unsigned long i;
		unsigned char* p;
		sc_bv<DATA_WIDTH> word;
   
		for(p = (unsigned char*)data, i = 0; i < len; i++, p++)
		{
			if(!(i%DATA_BYTES))
			{
				protocol->masterRead(addr, word);
			}

			// limitations of SystemC, need to do this weird casting
			*p = (sc_uint<8>)((sc_bv<8>)word.range(DATA_WIDTH-1,DATA_WIDTH-8));

			word = word << 8;
		}
	}
};

class	SlaveHardwareBusLinkAccess : public ISlaveHardwareBusLinkAccess, public sc_channel
{
	public:

	sc_port<ISlaveHardwareBusProtocol> protocol;

	SlaveHardwareBusLinkAccess(sc_module_name name) : sc_channel(name) {}

	void	SlaveWrite(int addr, const void* data, unsigned long len)
	{
		unsigned long i;
		unsigned char *p;
		sc_uint<DATA_WIDTH> word = 0;
   
		for(p = (unsigned char*)data, i = 0; i < len; i++, p++)
		{
			word = (word<<8) + *p;

			if(!((i+1)%DATA_BYTES))
			{
				protocol->slaveWrite(addr, word);
				word = 0;
			}
		}

		if(i%DATA_BYTES)
		{
			word <<= 8 * (DATA_BYTES - (i%DATA_BYTES));
			protocol->slaveWrite(addr, word);
		}
	}

	void	SlaveRead(int addr, void* data, unsigned long len)
	{
		unsigned long i;
		unsigned char* p;
		sc_bv<DATA_WIDTH> word;

		for(p = (unsigned char*)data, i = 0; i < len; i++, p++)
		{
			if(!(i%DATA_BYTES))
			{
				protocol->slaveRead(addr, word);
			}

			// limitations of SystemC, need to do this weird casting
			*p = (sc_uint<8>)((sc_bv<8>)word.range(DATA_WIDTH-1,DATA_WIDTH-8));

			word = word << 8;
		}
	}
};

#endif
