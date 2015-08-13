using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Storage;
using Windows.Storage.Pickers;
using Windows.Storage.Streams;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using FFmpegLib;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace MotionVectors
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        private FFmpegSDK _sdk = null;
        IRandomAccessStream _readStream;
        private StorageFile _logFile = null;
        DispatcherTimer _timer = new DispatcherTimer();


        public MainPage()
        {
            this.InitializeComponent();
            this.Loaded += MainPage_Loaded;

            _timer.Interval = TimeSpan.FromSeconds(1);
            _timer.Tick += _timer_Tick;
            _timer.Start();

        }

        private void _timer_Tick(object sender, object e)
        {
            if(_sdk != null && _sdk.FramesProcessed != 0)
            {
                Frame.Text = _sdk.FramesProcessed.ToString();
            }
        }

        private async void MainPage_Loaded(object sender, RoutedEventArgs e)
        {
            _sdk = FFmpegSDK.Initialize();
        }

        private async void OnOpenFile(object sender, RoutedEventArgs e)
        {
            FileOpenPicker filePicker = new FileOpenPicker();
            filePicker.ViewMode = PickerViewMode.Thumbnail;
            filePicker.SuggestedStartLocation = PickerLocationId.VideosLibrary;
            filePicker.FileTypeFilter.Add("*");

            // Show file picker so user can select a file
            StorageFile file = await filePicker.PickSingleFileAsync();

            if (file != null)
            {
                FileName.Text = file.Name;
                var nameModified = file.Name.Split('.')[0] + ".txt";
                _logFile = await Windows.Storage.ApplicationData.Current.LocalFolder.CreateFileAsync(nameModified, CreationCollisionOption.ReplaceExisting);
                _readStream = await file.OpenAsync(FileAccessMode.Read);
            }

        }

        private void OnStart(object sender, RoutedEventArgs e)
        {
            _sdk.ReadMotionFrames(_readStream,_logFile);
        }
    }
}
