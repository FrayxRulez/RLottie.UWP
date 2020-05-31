using RLottie;
using System;
using Windows.ApplicationModel;
using Windows.Storage;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;

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
                var result = await file.CopyAsync(ApplicationData.Current.LocalFolder, file.Name, NameCollisionOption.ReplaceExisting);
            }
            catch { }

            using (var lottie = CachedAnimation.LoadFromFile(ApplicationData.Current.LocalFolder.Path + $"\\{_stickers[index]}.tgs", false, true, null))
            {
                Pic.Source = lottie.RenderSync(0, 256, 256);
            }

            //var player = new LottieView
            //{
            //    Width = 200,
            //    Height = 200,
            //    Source = new Uri($"ms-appdata://local/{_stickers[index]}.tgs")
            //};

            //Panel.Children.Add(player);

            //index = (index + 1) % _stickers.Length;
        }

        private void Button2_Click(object sender, RoutedEventArgs e)
        {
            //Panel.Children.RemoveAt(0);

            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }
    }
}
