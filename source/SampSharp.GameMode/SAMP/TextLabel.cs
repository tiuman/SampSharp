﻿// SampSharp
// Copyright (C) 2014 Tim Potze
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
// 
// For more information, please refer to <http://unlicense.org>

using System;
using SampSharp.GameMode.Definitions;
using SampSharp.GameMode.Natives;
using SampSharp.GameMode.Pools;
using SampSharp.GameMode.World;

namespace SampSharp.GameMode.SAMP
{
    public class TextLabel : IdentifiedPool<TextLabel>, IIdentifyable
    {
        #region Fields

        /// <summary>
        ///     Gets an ID commonly returned by methods to point out that no TextLabel matched the requirements.
        /// </summary>
        public const int InvalidId = Misc.Invalid_3DTextId;

        private Color _color;
        private float _drawDistance;
        private Vector _position;
        private bool _testLOS;
        private string _text;
        private int _virtualWorld;

        #endregion

        #region Properties

        public virtual Color Color
        {
            get { return _color; }
            set
            {
                _color = value;
                Native.Update3DTextLabelText(Id, Color, Text);
            }
        }

        public virtual string Text
        {
            get { return _text; }
            set
            {
                _text = value;
                Native.Update3DTextLabelText(Id, Color, Text);
            }
        }

        public virtual Vector Position
        {
            get { return _position; }
            set
            {
                _position = value;
                Dispose();
                Id = Native.Create3DTextLabel(Text, Color, Position, DrawDistance, VirtualWorld, TestLOS);
            }
        }

        public virtual float DrawDistance
        {
            get { return _drawDistance; }
            set
            {
                _drawDistance = value;
                Dispose();
                Id = Native.Create3DTextLabel(Text, Color, Position, DrawDistance, VirtualWorld, TestLOS);
            }
        }

        public virtual int VirtualWorld
        {
            get { return _virtualWorld; }
            set
            {
                _virtualWorld = value;
                Dispose();
                Id = Native.Create3DTextLabel(Text, Color, Position, DrawDistance, VirtualWorld, TestLOS);
            }
        }

        public virtual bool TestLOS
        {
            get { return _testLOS; }
            set
            {
                _testLOS = value;
                Dispose();
                Id = Native.Create3DTextLabel(Text, Color, Position, DrawDistance, VirtualWorld, TestLOS);
            }
        }

        public virtual int Id { get; private set; }

        #endregion

        #region Constructors

        public TextLabel(string text, Color color, Vector position, float drawDistance, int virtualWorld, bool testLOS)
        {
            _text = text;
            _color = color;
            _position = position;
            _drawDistance = drawDistance;
            _virtualWorld = virtualWorld;
            _testLOS = testLOS;
            Id = Native.Create3DTextLabel(text, color, position, drawDistance, virtualWorld, testLOS);
        }

        public TextLabel(string text, Color color, Vector position, float drawDistance, int virtualWorld)
            : this(text, color, position, drawDistance, virtualWorld, true)
        {
        }

        public TextLabel(string text, Color color, Vector position, float drawDistance)
            : this(text, color, position, drawDistance, -1, true)
        {
        }

        #endregion

        #region Methods

        protected override void Dispose(bool disposing)
        {
            base.Dispose(disposing);

            Native.Delete3DTextLabel(Id);
        }

        public virtual void AttachTo(Player player, Vector offset)
        {
            CheckDisposure();

            if (player == null)
                throw new NullReferenceException("player cannot be null");

            Native.Attach3DTextLabelToPlayer(Id, player.Id, offset.X, offset.Y, offset.Z);
        }

        public virtual void AttachTo(Vehicle vehicle, Vector offset)
        {
            CheckDisposure();

            if (vehicle == null)
                throw new NullReferenceException("vehicle cannot be null");

            Native.Attach3DTextLabelToVehicle(Id, vehicle.Id, offset.X, offset.Y, offset.Z);
        }

        #endregion
    }
}