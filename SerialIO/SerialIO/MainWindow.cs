using System;
using Gtk;
using System.IO.Ports;
using System.Threading;
using System.Globalization;

public partial class MainWindow : Gtk.Window
{
	SerialPort sp;
	Thread readThread;
	bool quit;
	
	public MainWindow () : base(Gtk.WindowType.Toplevel)
	{
		Build ();
	}

	protected void OnDeleteEvent (object sender, DeleteEventArgs a)
	{
		quit = true;
		readThread.Join();
		sp.Close();
		Application.Quit ();
		a.RetVal = true;
	}

	protected virtual void OnStartClicked (object sender, System.EventArgs e)
	{
		sp = new SerialPort(devtty.Text, int.Parse(baudrate.Text), Parity.None, 8, StopBits.One);
		sp.Open();
		quit = false;
		readThread = new Thread(new ThreadStart(ReadFromSerialPort));
		readThread.Start();
	}
	
	protected virtual void OnsendClicked (object sender, System.EventArgs e)
	{
		byte[] ba = new byte[1];
		ba[0] = byte.Parse(serialout.Buffer.Text, NumberStyles.HexNumber);
		sp.Write(ba, 0, 1);
	}
	
	protected void ReadFromSerialPort()
	{
		try
		{
			while(!quit)
			{
				byte[] ba = new byte[1];
				sp.Read(ba, 0, 1);
				serialin.Buffer.Text += String.Format("{0:X} is {1}\n", Convert.ToInt32(ba[0]),
				                                      ((ba[0] >= 32) && (ba[0] <= 126)) ? Convert.ToChar(ba[0]) : '.');
			}
		}
		catch (Exception e){
			Console.WriteLine(e.StackTrace);
		}
	}
	
}
