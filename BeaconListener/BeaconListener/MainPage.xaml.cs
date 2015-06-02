using Common;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.ApplicationModel.Core;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Storage;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace BeaconListener
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        private BackgroundManager _backgroundManager;
        private BeaconEngine _engine;

        public MainPage()
        {
            this.InitializeComponent();
            LogEntryItemCollection = new ObservableCollection<LogEntryItem>();

            _backgroundManager = new BackgroundManager();


            _engine = new BeaconEngine(false);
            _engine.NewBeacons += OnNewBeacons;
        }

        private ObservableCollection<LogEntryItem> LogEntryItemCollection
        {
            get
            {
                return (ObservableCollection<LogEntryItem>)GetValue(LogEntryItemCollectionProperty);
            }
            set
            {
                SetValue(LogEntryItemCollectionProperty, value);
            }
        }

        private static readonly DependencyProperty LogEntryItemCollectionProperty =
            DependencyProperty.Register("LogEntryItemCollection", typeof(ObservableCollection<LogEntryItem>), typeof(MainPage),
                new PropertyMetadata(null));

        private async void OnNewBeacons(object sender, string e)
        {
            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal,
                () =>
                {
                    AddLogEntry(e);
                });
        }

        private void onToggleBackgroundTaskButton(object sender, RoutedEventArgs e)
        {
            if (_backgroundManager != null)
            {
                if (_backgroundManager.IsBackgroundTaskRegistered)
                {
                    _backgroundManager.UnregisterBackgroundTask();
                    AddLogEntry("Background task is now unregistered");
                    UpdateLabels();
                }
                else
                {
                    if(_backgroundManager.RegisterAdvertisementWatcherBackgroundTask())
                    {
                        AddLogEntry("Background task is registered.");
                    } else
                    {
                        AddLogEntry("Background task registration failed");
                    }
                }
            }
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);
            UpdateLabels();
        }

        private void UpdateLabels()
        {
            if (_backgroundManager != null)
            {
                toggleBackgroundTaskButton.Label = _backgroundManager.IsBackgroundTaskRegistered ? "turn off notifications" : "turn on notifications";
            }
        }

        private void AddLogEntry(string message)
        {
            LogEntryItem logEntryItem = new LogEntryItem(message);
            LogEntryItemCollection.Insert(0, logEntryItem);
        }
    }

    public class LogEntryItem
    {
        public string Timestamp
        {
            get;
            private set;
        }

        public string Message
        {
            get;
            set;
        }

        public LogEntryItem(string message)
        {
            Timestamp = string.Format("{0:H:mm:ss}", DateTime.Now);
            Message = message;
        }
    }
}
