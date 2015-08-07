using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Text;
using System.Threading.Tasks;
using Windows.ApplicationModel.Background;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Storage.Streams;

namespace Common
{
    public class CommonConstants
    {
        public const int SignalStrengthFilterInRangeThresholdInDBm = -120;
        public const int SignalStrengthFilterOutOfRangeThresholdInDBm = -127;
        public static readonly string UUIDSpace1 = "7367672374000000ffff0000ffff00";
        public const string ManufacturerCompanyIdAsString = "4C00";
        public const string DefaultBeaconTypeAndDataLengthAsString = "0215";
        public const byte DataSectionDataType = 0xFF;
    }

    public class BackgroundManager
    {
        private static readonly string AdvertisementWatcherBackgroundTaskName = "AdvertisementWatcherBackgroundTask";
        private static readonly string AdvertisementWatcherBackgroundTaskEntryPoint = "AdvertisementWatcherBackgroundTask.AdvertisementWatcherBackgroundTask";

        public bool IsBackgroundTaskRegistered
        {
            get
            {
                return (BackgroundTaskRegistered(AdvertisementWatcherBackgroundTaskName));
            }
        }

        /// <summary>
        /// Unregisters Bluetooth advertisement listener
        /// </summary>
        /// <returns></returns>
        public void UnregisterBackgroundTask()
        {
            foreach (var taskValue in BackgroundTaskRegistration.AllTasks.Values)
            {
                if (taskValue.Name.Equals(AdvertisementWatcherBackgroundTaskName))
                {
                    taskValue.Unregister(true);
                }
            }
        }

        /// <summary>
        /// Registers Bluetooth advertisement listener
        /// </summary>
        /// <returns></returns>
        public bool RegisterAdvertisementWatcherBackgroundTask()
        {
            if (BackgroundTaskRegistered(AdvertisementWatcherBackgroundTaskName))
            {
                return true;
            }
            else
            {
                BackgroundTaskBuilder backgroundTaskBuilder = new BackgroundTaskBuilder();

                backgroundTaskBuilder.Name = AdvertisementWatcherBackgroundTaskName;
                backgroundTaskBuilder.TaskEntryPoint = AdvertisementWatcherBackgroundTaskEntryPoint;

                IBackgroundTrigger trigger = null;

                BluetoothLEAdvertisementWatcherTrigger advertisementWatcherTrigger =
                    new BluetoothLEAdvertisementWatcherTrigger();

                //This filter includes UUIDSpace 7367672374000000ffff0000ffff00xx
                var pattern = BEACONIDToAdvertisementBytePattern(CommonConstants.UUIDSpace1);
                advertisementWatcherTrigger.AdvertisementFilter.BytePatterns.Add(pattern);

                //Using MaxSamplingInterval as SamplingInterval ensures that we get an event only when entering or exiting from the range of the beacon
                advertisementWatcherTrigger.SignalStrengthFilter.SamplingInterval = advertisementWatcherTrigger.MaxSamplingInterval;
                advertisementWatcherTrigger.SignalStrengthFilter.InRangeThresholdInDBm = CommonConstants.SignalStrengthFilterInRangeThresholdInDBm;
                advertisementWatcherTrigger.SignalStrengthFilter.OutOfRangeThresholdInDBm = CommonConstants.SignalStrengthFilterOutOfRangeThresholdInDBm;
                advertisementWatcherTrigger.SignalStrengthFilter.OutOfRangeTimeout = TimeSpan.FromMilliseconds(9000);

                trigger = advertisementWatcherTrigger;

                backgroundTaskBuilder.SetTrigger(trigger);

                try
                {
                    BackgroundTaskRegistration backgroundTaskRegistration = backgroundTaskBuilder.Register();
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine("RegisterAdvertisementWatcherBackgroundTask(): Failed to register: " + ex);
                    return false;
                }
                return true;
            }
        }
        /// <summary>
        /// Converts the given UUID string to BluetoothLEAdvertisementBytePattern.
        /// </summary>
        /// <param name="hexString"></param>
        /// <returns></returns>
        private BluetoothLEAdvertisementBytePattern BEACONIDToAdvertisementBytePattern(string BEACONID)
        {
            if (BEACONID.Length % 2 != 0)
            {
                BEACONID = BEACONID.Remove(BEACONID.Length - 1);
            }

            StringBuilder stringBuilder = new StringBuilder();
            stringBuilder.Append(CommonConstants.ManufacturerCompanyIdAsString);
            stringBuilder.Append(CommonConstants.DefaultBeaconTypeAndDataLengthAsString);
            stringBuilder.Append(BEACONID.ToUpper());

            byte[] data = HexStringToByteArray(stringBuilder.ToString());

            BluetoothLEAdvertisementBytePattern pattern = new BluetoothLEAdvertisementBytePattern();
            pattern.DataType = CommonConstants.DataSectionDataType;
            pattern.Offset = 0;
            pattern.Data = data.AsBuffer();

            return pattern;
        }

        /// <summary>
        /// Converts the given hex string to byte array.
        /// </summary>
        /// <param name="hexString"></param>
        /// <returns></returns>
        private static byte[] HexStringToByteArray(string hexString)
        {
            return Enumerable.Range(0, hexString.Length)
                .Where(x => x % 2 == 0)
                .Select(x => Convert.ToByte(hexString.Substring(x, 2), 16))
                .ToArray();
        }

        /// <summary>
        /// Checks if a background task with the given name is registered.
        /// </summary>
        /// <param name="taskName">The name of the background task.</param>
        /// <returns>True, if registered. False otherwise.</returns>
        private bool BackgroundTaskRegistered(string taskName)
        {
            bool registered = false;

            foreach (var taskValue in BackgroundTaskRegistration.AllTasks.Values)
            {
                if (taskValue.Name.Equals(taskName))
                {
                    registered = true;
                    break;
                }
            }

            return registered;
        }
    }
}
