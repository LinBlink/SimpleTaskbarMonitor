using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Threading;
using LibreHardwareMonitor.Hardware;

// Isolated sensor worker for the Lite host: no plugin API or old application DLL.
internal static class TemperatureWorker {
    static string Numeric(float? value) {
        return value.HasValue && !float.IsNaN(value.Value) && !float.IsInfinity(value.Value) && value.Value>=0 && value.Value<=150
            ? value.Value.ToString("0.0",CultureInfo.InvariantCulture) : "-1";
    }
    static void Sensors(IHardware hardware, List<ISensor> cpu, List<ISensor> gpu) {
        hardware.Update();
        foreach(var sensor in hardware.Sensors) {
            if(sensor.SensorType!=SensorType.Temperature || !sensor.Value.HasValue) continue;
            if(hardware.HardwareType==HardwareType.Cpu) cpu.Add(sensor);
            if(hardware.HardwareType==HardwareType.GpuNvidia || hardware.HardwareType==HardwareType.GpuAmd || hardware.HardwareType==HardwareType.GpuIntel) gpu.Add(sensor);
        }
        foreach(var sub in hardware.SubHardware) Sensors(sub,cpu,gpu);
    }
    static float? Select(List<ISensor> sensors, string preferred, bool cpu) {
        foreach(var sensor in sensors) if(string.Equals(sensor.Name,preferred,StringComparison.OrdinalIgnoreCase)) return sensor.Value;
        string[] names=cpu ? new[]{"Core Average","CPU Package","CPU (Tctl/Tdie)","CPU Die (average)"} : new[]{"GPU Core","GPU Temperature"};
        foreach(var name in names) foreach(var sensor in sensors) if(string.Equals(sensor.Name,name,StringComparison.OrdinalIgnoreCase)) return sensor.Value;
        return sensors.Count>0 ? sensors[0].Value : null;
    }
    static void Save(string file, float? cpu, float? gpu, string status) {
        string text="[temperature]\r\nupdated="+DateTimeOffset.UtcNow.ToUnixTimeSeconds()+"\r\ncpu="+Numeric(cpu)+"\r\ngpu="+Numeric(gpu)+"\r\nstatus="+status.Replace('\r',' ').Replace('\n',' ')+"\r\n";
        string temporary=file+".new";
        File.WriteAllText(temporary,text,Encoding.Unicode);
        if(File.Exists(file)) File.Replace(temporary,file,null); else File.Move(temporary,file);
    }
    static int Main(string[] args) {
        if(args.Length<4) return 2;
        string file=Path.GetFullPath(args[0]);
        int parentId=int.Parse(args[1],CultureInfo.InvariantCulture);
        bool cpu=args[2]=="1", gpu=args[3]=="1";
        string preferred=args.Length>4 ? args[4] : "Core Average";
        var computer=new Computer {IsCpuEnabled=cpu,IsGpuEnabled=gpu};
        Process parent=null;
        var stop=new EventWaitHandle(false,EventResetMode.ManualReset,"Local\\TrafficMonitor.Sensors.Stop."+parentId);
        try {
            parent=Process.GetProcessById(parentId);
            computer.Open();
            while(!parent.HasExited && !stop.WaitOne(0)) {
                try {
                    var cpuSensors=new List<ISensor>(); var gpuSensors=new List<ISensor>();
                    foreach(var hardware in computer.Hardware) Sensors(hardware,cpuSensors,gpuSensors);
                    Save(file,Select(cpuSensors,preferred,true),Select(gpuSensors,"GPU Core",false),"OK");
                } catch(Exception e) { Save(file,null,null,e.GetType().Name); }
                stop.WaitOne(1500);
            }
            return 0;
        } catch(Exception e) {
            try{Save(file,null,null,e.GetType().Name+": "+e.Message);}catch{}
            return 1;
        } finally {try{computer.Close();}catch{} if(parent!=null) parent.Dispose();stop.Dispose();}
    }
}
