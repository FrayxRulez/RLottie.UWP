using RLottie;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading;
using Unigram.Controls;
using Windows.ApplicationModel;
using Windows.Graphics.Imaging;
using Windows.Storage;
using Windows.Storage.Streams;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media.Imaging;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace App1
{
    [ComImport]
    [Guid("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    unsafe interface IMemoryBufferByteAccess
    {
        void GetBuffer(out byte* buffer, out uint capacity);
    }

    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        public MainPage()
        {
            this.InitializeComponent();
        }

        private int index = 0;
        private string[] _stickers = new string[]
        {
            "1258816259753938",
            "WalletIntroLoading",
            "2026484562321736022"
        };

        private async void Button_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var file = await Package.Current.InstalledLocation.GetFileAsync($"{_stickers[index]}.tgs");
                var result = await file.CopyAsync(ApplicationData.Current.LocalFolder, file.Name, NameCollisionOption.ReplaceExisting);
            }
            catch { }

            var player = new LottieView
            {
                Width = 200,
                Height = 200,
                Source = new Uri($"ms-appdata://local/{_stickers[index]}.tgs")
            };

            Panel.Children.Add(player);

            index = (index + 1) % _stickers.Length;
        }

        private void Button2_Click(object sender, RoutedEventArgs e)
        {
            Panel.Children.RemoveAt(0);

            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }
    }
}
