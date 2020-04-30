using Microsoft.Graphics.Canvas;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Unigram.Controls;
using Windows.ApplicationModel;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Storage;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace App1
{
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
                var result = await file.CopyAsync(ApplicationData.Current.LocalFolder, file.Name, NameCollisionOption.FailIfExists);
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
        }
    }
}
