using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth.Background;
using Windows.Storage;

namespace Common
{
    public class BeaconEngine
    {
        public event EventHandler<string> NewBeacons;

        private const string KeyNewMessages = "new_messages_arrived";
        private const int CheckPendingBeaconActionsFromBackgroundIntervalInMilliseconds = 1000;
        private const byte SecondBeaconDataSectionDataType = 0xFF;
        private const string KeyTransferFileName = "Transfer.txt";

        private bool _background;
        private Semaphore _semaphore = new Semaphore(1, 1, "BeaconMutex");
        private ApplicationDataContainer _localSettings;
        private Timer _checkNewBeaconsTimer;
        private IList<string> _beaconEntries = new List<string>();

        public BeaconEngine(bool background)
        {
            _background = background;
            _localSettings = ApplicationData.Current.LocalSettings;

            if (!background)
            {
                _checkNewBeaconsTimer =
                new Timer(OnCheckNewBeaconEntries, null,
                    CheckPendingBeaconActionsFromBackgroundIntervalInMilliseconds,
                    CheckPendingBeaconActionsFromBackgroundIntervalInMilliseconds);
            }
        }

        /// <summary>
        /// Parses advertisement data and stores them to a file.
        /// Function is called from the background task.
        /// </summary>
        /// <param name="triggerDetails">Trigger details</param>
        /// <returns></returns>
        public async Task ProcessTriggerDetails(BluetoothLEAdvertisementWatcherTriggerDetails triggerDetails)
        {
            if (triggerDetails != null)
            {
                foreach (var bleAdvertisementReceivedEventArgs in triggerDetails.Advertisements)
                {
                    if(bleAdvertisementReceivedEventArgs.Advertisement != null)
                    {
                        if (bleAdvertisementReceivedEventArgs.Advertisement.DataSections != null)
                        {
                            foreach (var item in bleAdvertisementReceivedEventArgs.Advertisement.DataSections)
                            {
                                bool exit = false;
                                if (bleAdvertisementReceivedEventArgs.RawSignalStrengthInDBm == triggerDetails.SignalStrengthFilter.OutOfRangeThresholdInDBm)
                                    exit = true;

                                SaveBeaconToBeaconEntriesList(item.Data.ToArray(),exit);
                            }
                        }
                    }
                }
            }

            if (_beaconEntries != null && _beaconEntries.Count > 0)
            {
                await WriteEntriesToFileAsync();
            }
        }

        /// <summary>
        /// Parses a beacon from a byte array and stores it to _beaconEntries
        /// </summary>
        /// <param name="data">Advertisement in byte array</param>
        /// <returns></returns>
        private void SaveBeaconToBeaconEntriesList([ReadOnlyArray] byte[] data,bool exit)
        {
            if(data.Length > 20)
            {
                string msg = exit ? "exit: " : "enter: ";
                var uuid = BitConverter.ToString(data, 4, 16);
                _beaconEntries.Add(msg+uuid);
            }
        }

        /// <summary>
        /// Parses a beacon from a byte array and stores it to _beaconEntries
        /// </summary>
        /// <param name="data">Advertisement in byte array</param>
        /// <returns></returns>
        private async Task WriteEntriesToFileAsync()
        {
            if (_semaphore.WaitOne(1000))
            {
                try
                {
                    _localSettings.Values[KeyNewMessages] = true;
                    var file = await Windows.Storage.ApplicationData.Current.LocalFolder.CreateFileAsync(KeyTransferFileName, CreationCollisionOption.OpenIfExists);
                    await Windows.Storage.FileIO.AppendLinesAsync(file, _beaconEntries);
                    _beaconEntries.Clear();
                }
                catch { }
                finally
                {
                    _semaphore.Release();
                }
            }
        }

        /// <summary>
        /// Called on the foreground. Checks if there are new items on transfer file and fires NewBeacons event
        /// </summary>
        /// <returns></returns>
        private async void OnCheckNewBeaconEntries(object state)
        {
            if (_semaphore.WaitOne(1000))
            {
                try
                {
                    if (_localSettings.Values.ContainsKey(KeyNewMessages))
                    {
                        if(true == (bool)_localSettings.Values[KeyNewMessages])
                        {
                            if(NewBeacons != null)
                            {
                                var file = await Windows.Storage.ApplicationData.Current.LocalFolder.TryGetItemAsync(KeyTransferFileName);

                                if (file != null)
                                {
                                    _beaconEntries = await Windows.Storage.FileIO.ReadLinesAsync(file as IStorageFile);
                                    await file.DeleteAsync();
                                    _localSettings.Values[KeyNewMessages] = false;
                                }
                            }
                        }
                    }
                }
                catch { }
                finally
                {
                    _semaphore.Release();
                }
            }

            if(_beaconEntries != null && _beaconEntries.Count > 0)
            {
                foreach (var item in _beaconEntries)
                {
                    NewBeacons(this, item);
                }
                _beaconEntries.Clear();
            }
        }
    }
}
