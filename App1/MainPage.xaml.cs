using Microsoft.Graphics.Canvas;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
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

        private RLottie.Animation _json;
        private CanvasBitmap[] _frames;
        private int _index;

        private async void Button_Click(object sender, RoutedEventArgs e)
        {
            var watch1 = Stopwatch.StartNew();

            var uri = new Uri("file:///" + @"C:\Users\Рома\AppData\Local\Packages\38833FF26BA1D.UnigramPreview_g9c9v27vpyspw\LocalState\0\stickers\1220028319907446914.tgs");
            var path = UriToPath(uri);

            var test = Decompress("1258816259753938.tgs");

            _json = RLottie.Animation.LoadFromFile("1258816259753938.tgs");
            _frames = new CanvasBitmap[_json.TotalFrame];
        }

        private string UriToPath(Uri uri)
        {
            if (uri == null)
            {
                return null;
            }

            switch (uri.Scheme)
            {
                case "ms-appx":
                    return Path.Combine(uri.Segments.Select(x => x.Trim('/')).ToArray());
                case "ms-appdata":
                    switch (uri.Host)
                    {
                        case "local":
                            return Path.Combine(new[] { ApplicationData.Current.LocalFolder.Path }.Union(uri.Segments.Select(x => x.Trim('/'))).ToArray());
                        case "temp":
                            return Path.Combine(new[] { ApplicationData.Current.TemporaryFolder.Path }.Union(uri.Segments.Select(x => x.Trim('/'))).ToArray());
                    }
                    break;
                case "file":
                    return uri.LocalPath;
            }

            return null;
        }


        private string Decompress(string path)
        {
            string text;
            using (var stream = File.OpenRead(path))
            {
                var decompressedFileStream = new System.IO.MemoryStream();
                using (var decompressionStream = new GZipStream(stream, CompressionMode.Decompress))
                {
                    decompressionStream.CopyTo(decompressedFileStream);
                }

                decompressedFileStream.Seek(0, SeekOrigin.Begin);

                using (var reader = new StreamReader(decompressedFileStream))
                {
                    text = reader.ReadToEnd();
                }
            }

            return text;
        }
    }
}
