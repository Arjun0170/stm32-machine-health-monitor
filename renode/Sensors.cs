using System;
using Antmicro.Renode.Peripherals.I2C;
using Antmicro.Renode.Peripherals.SPI;
using Antmicro.Renode.Core;
namespace Antmicro.Renode.Peripherals.Sensors
{
    public class DemoTemperature : II2CPeripheral
    {
        public DemoTemperature() { Reset(); }
        public int Temperature { get; set; }
        public bool Available { get; set; }
        public int Reads { get; private set; }
        private byte pointer;
        public void Reset() { Temperature=250;Available=true;Reads=0;pointer=0; }
        public void Write(byte[] data) { if(data.Length>0) pointer=data[0]; }
        public byte[] Read(int count=1) {
            Reads++; if(!Available) return new byte[0];
            var result=new byte[count];
            for(int i=0;i<count;i++) { int r=pointer++; result[i]=r==0?(byte)Temperature:r==1?(byte)(Temperature>>8):r==2?(byte)0x54:(byte)0; }
            return result;
        }
        public void FinishTransmission() {}
    }
    public class DemoAccelerometer : ISPIPeripheral
    {
        public DemoAccelerometer() { Reset(); }
        public int X { get; set; }
        public int Y { get; set; }
        public int Z { get; set; }
        public int Reads { get; private set; }
        private int pointer; private bool command;
        public void Reset() { X=100;Y=-50;Z=1000;Reads=0;pointer=0;command=true; }
        public byte Transmit(byte value) {
            if(command) { command=false;pointer=value&0x7f;Reads++;return 0; }
            int p=pointer++,v=p<2?X:p<4?Y:Z;
            return p<6?(byte)(v>>(8*(p&1))):p==6?(byte)0x41:(byte)0;
        }
        public void FinishTransmission() { command=true; }
    }
}
