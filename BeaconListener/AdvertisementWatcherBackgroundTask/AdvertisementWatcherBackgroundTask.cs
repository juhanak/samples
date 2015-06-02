using Common;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.ApplicationModel.Background;
using Windows.Devices.Bluetooth.Background;

namespace AdvertisementWatcherBackgroundTask
{
    public sealed class AdvertisementWatcherBackgroundTask : IBackgroundTask
    {
        BeaconEngine _eng;

        public AdvertisementWatcherBackgroundTask()
        {
            _eng = new BeaconEngine(true);
        }

        public async void Run(IBackgroundTaskInstance taskInstance)
        {
            var def = taskInstance.GetDeferral();

            var triggerDetails = taskInstance.TriggerDetails as BluetoothLEAdvertisementWatcherTriggerDetails;

            if(triggerDetails != null)
            {
                await _eng.ProcessTriggerDetails(triggerDetails);
            }
            
            def.Complete();
        }

    }
}
