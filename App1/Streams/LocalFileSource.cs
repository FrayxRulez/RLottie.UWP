﻿//
// Copyright Fela Ameghino 2015-2023
//
// Distributed under the GNU General Public License v3.0. (See accompanying
// file LICENSE or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
//
using System;
using Windows.ApplicationModel;
using Path = System.IO.Path;

namespace Telegram.Streams
{
    public class LocalFileSource : AnimatedImageSource
    {
        private long _offset;

        //public LocalFileSource(File file)
        //{
        //    FilePath = file.Local.Path;
        //    FileSize = file.Size;

        //    Id = file.Id;
        //}

        public LocalFileSource(string path)
        {
            FilePath = UriToPath(path);
        }

        private static string UriToPath(string uri)
        {
            var split = uri.Split('/', StringSplitOptions.RemoveEmptyEntries);

            switch (split[0])
            {
                case "ms-appx:":
                    split[0] = Package.Current.InstalledLocation.Path;
                    return Path.Combine(split);
                default:
                    return uri;
            }
        }

        public override string FilePath { get; }
        public override long FileSize { get; }

        public override int Id { get; }

        public override long Offset => _offset;

        public override void SeekCallback(long offset)
        {
            _offset = offset;
        }

        public override void ReadCallback(long count)
        {
            // Nothing
        }

        public override bool Equals(object obj)
        {
            if (obj is LocalFileSource y)
            {
                return y.FilePath == FilePath;
            }

            return base.Equals(obj);
        }

        public override int GetHashCode()
        {
            return FilePath.GetHashCode();
        }
    }
}
