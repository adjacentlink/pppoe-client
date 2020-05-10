 . /home/jgiovatto/Devel/environments/emane.env

DEV=letce0

emaneevent-location 1  -t 0 latitude=40.0 longitude=-74.0 altitude=1000  -i $DEV
emaneevent-location 2  -t 0 latitude=40.0 longitude=-74.0 altitude=2000  -i $DEV
emaneevent-location 3  -t 0 latitude=40.0 longitude=-74.0 altitude=3000  -i $DEV
emaneevent-location 4  -t 0 latitude=40.0 longitude=-74.0 altitude=4000  -i $DEV
emaneevent-location 5  -t 0 latitude=40.0 longitude=-74.0 altitude=5000  -i $DEV
emaneevent-location 6  -t 0 latitude=40.0 longitude=-74.0 altitude=6000  -i $DEV
emaneevent-location 7  -t 0 latitude=40.0 longitude=-74.0 altitude=7000  -i $DEV
emaneevent-location 8  -t 0 latitude=40.0 longitude=-74.0 altitude=8000  -i $DEV
emaneevent-location 9  -t 0 latitude=40.0 longitude=-74.0 altitude=9000  -i $DEV
emaneevent-location 10 -t 0 latitude=40.0 longitude=-74.0 altitude=10000 -i $DEV

# set all to low loss
emaneevent-pathloss -i $DEV 1:10 80

