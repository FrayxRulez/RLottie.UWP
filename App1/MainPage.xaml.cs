using RLottie;
using System;
using System.IO;
using System.Threading.Tasks;
using Unigram.Controls;
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

                await Task.Run(() => LottieAnimation.LoadFromFile(result.Path, false, null).RenderSync(Path.Combine(ApplicationData.Current.LocalFolder.Path, "test.png"), 512, 512, 0));
            }
            catch { }

            return;

            var player = new LottieView
            {
                AutoPlay = true,
                Width = 200,
                Height = 200,
                Source = new Uri($"ms-appdata://local/{_stickers[index]}.tgs")
            };

            Panel.Children.Add(player);

            index = (index + 1) % _stickers.Length;
        }

        private void Button2_Click(object sender, RoutedEventArgs e)
        {
            if (Panel.Children.Count > 0)
            {
                Panel.Children.RemoveAt(0);
            }

            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }
    }
}
